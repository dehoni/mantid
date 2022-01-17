# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
from os import path

from mantid.simpleapi import Load, logger, EnggEstimateFocussedBackground, Minus, AverageLogData, SetUncertainties, \
    CreateEmptyTableWorkspace, GroupWorkspaces, DeleteWorkspace, DeleteTableRows, RenameWorkspace, CreateWorkspace
from mantidqtinterfaces.Engineering.gui.engineering_diffraction.settings.settings_helper import get_setting
from mantidqtinterfaces.Engineering.gui.engineering_diffraction.tabs.common import output_settings
from mantid.api import AnalysisDataService as ADS
from mantid.api import TextAxis
from mantid.kernel import UnitConversion, DeltaEModeType, UnitParams
from matplotlib.pyplot import subplots
from numpy import full, nan, max, array, vstack, argsort
from itertools import chain
from collections import defaultdict, OrderedDict
from re import findall, sub


class FittingWorkspaceRecord:
    """ Record that maps the workspace the user has loaded to the derived background-subtracted workspace"""
    def __init__(self, **kwargs):
        self.loaded_ws = kwargs.get('loaded_ws', None)
        self.bgsub_ws_name = kwargs.get('bgsub_ws_name', None)
        self.bgsub_ws = kwargs.get('bgsub_ws', None)
        self.bg_params = kwargs.get('bg_params', [])

    def get_bg_active(self):
        if self.bgsub_ws and self.bg_params[0]:
            # first element is isSub checkbox
            return True
        else:
            return False

    def get_active_ws(self):
        if self.get_bg_active():
            return self.bgsub_ws
        else:
            return self.loaded_ws

    def __setattr__(self, key, value):
        if key == 'bg_params':
            if not isinstance(value, list):
                raise AttributeError("bg_params must be a list")

        self.__dict__[key] = value  # initialize self.key


class FittingWorkspaceRecordContainer:
    def __init__(self):
        self.dict = OrderedDict()

    def __getitem__(self, key):
        return self.dict[key]

    def __len__(self):
        return len(self.dict)

    def __bool__(self):
        return len(self.dict)>0

    def get(self, key, default_value):
        return self.dict.get(key, default_value)

    def add(self, ws_name, **kwargs):
        self.dict[ws_name] = FittingWorkspaceRecord(**kwargs)

    def add_from_names_dict(self, names_dict):
        for key, value in names_dict.items():
            self.add(key, bgsub_ws_name=value[0], bg_params=value[1])

    def get_loaded_workpace_names(self):
        return list(self.dict.keys())

    def get_bgsub_workpace_names(self):
        return [w.bgsub_ws_name for w in self.dict.values()]

    # Set of methods that each return a two column dictionary for the various fields in FittingWorkspaceRecord
    def get_loaded_ws_dict(self):
        return dict([(key, value.loaded_ws) for key, value in self.dict.items()])

    def get_bgsub_ws_dict(self):
        return dict([(key, value.bgsub_ws) for key, value in self.dict.items()])

    def get_bgsub_ws_name_dict(self):
        return dict([(key, value.bgsub_ws_name) for key, value in self.dict.items()])

    def get_bg_params_dict(self):
        return dict([(key, value.bg_params) for key, value in self.dict.items()])

    def get_active_ws_name_list(self):
        return [self.get_active_ws_name(key) for key, value in self.dict.items()]

    def get_active_ws_dict(self):
        return dict([(self.get_active_ws_name(key), value.get_active_ws()) for key, value in self.dict.items()])

    def get_ws_names_dict(self):
        return dict([(key, [value.bgsub_ws_name, value.bg_params]) for key, value in self.dict.items()])

    def get_loaded_workspace_name_from_bgsub(self, bgsub_ws_name):
        return next((key for key, val in self.dict.items() if val.bgsub_ws_name == bgsub_ws_name), None)

    def get_active_ws_name(self, ws_name):
        ws_rec = self.dict[ws_name]
        if ws_rec.get_bg_active():
            return ws_rec.bgsub_ws_name
        else:
            return ws_name

    def rename(self, old_ws_name, new_ws_name):
        ws_loaded = self.dict.get(old_ws_name, None)
        if ws_loaded:
            self.dict[new_ws_name] = self.pop(old_ws_name)
        else:
            ws_loaded_name = self.get_loaded_workspace_name_from_bgsub(old_ws_name)
            if ws_loaded_name:
                self.dict[ws_loaded_name].bgsub_ws_name = new_ws_name

    def replace_workspace(self, name, workspace):
        ws_loaded = self.dict.get(name, None)
        if ws_loaded:
            self.dict[name].loaded_ws = workspace
        else:
            ws_loaded_name = self.get_loaded_workspace_name_from_bgsub(name)
            if ws_loaded_name:
                self.dict[ws_loaded_name].bgsub_ws = workspace

    def pop(self, ws_name):
        return self.dict.pop(ws_name)

    def clear(self):
        self.dict.clear()


class FittingDataModel(object):
    def __init__(self):
        self._log_names = []
        self._log_workspaces = None # GroupWorkspace
        self._log_values = dict()  # {ws_name: {log_name: [avg, er]} }
        self._fit_results = {}  # {WorkspaceName: fit_result_dict}
        self._fit_workspaces = None
        self._last_added = []  # List of workspace names loaded in the last load action.
        self._data_workspaces = FittingWorkspaceRecordContainer()
        self.inspect_bg_fig = None

    def restore_files(self, ws_names):
        self._data_workspaces.add_from_names_dict(ws_names)
        for ws_name in ws_names:
            try:
                ws = ADS.retrieve(ws_name)
                if ws.getNumberHistograms() == 1:
                    bgsubws = None
                    if self._data_workspaces[ws_name].bg_params:
                        bgsubws= ADS.retrieve(self._data_workspaces[ws_name].bgsub_ws_name)
                    self._last_added.append(ws_name)
                    self._data_workspaces[ws_name].loaded_ws = ws
                    self._data_workspaces[ws_name].bgsub_ws = bgsubws
                else:
                    logger.warning(
                        f"Invalid number of spectra in workspace {ws_name}. Skipping restoration of workspace.")
            except RuntimeError as e:
                logger.error(
                    f"Failed to restore workspace: {ws_name}. Error: {e}. \n Continuing loading of other files.")
        self.update_log_workspace_group()

    def load_files(self, filenames_string):
        self._last_added = []
        filenames = [name.strip() for name in filenames_string.split(",")]
        for filename in filenames:
            ws_name = self._generate_workspace_name(filename)
            if ws_name not in self._data_workspaces.get_loaded_workpace_names():
                try:
                    if not ADS.doesExist(ws_name):
                        ws = Load(filename, OutputWorkspace=ws_name)
                    else:
                        ws = ADS.retrieve(ws_name)
                    if ws.getNumberHistograms() == 1:
                        self._last_added.append(ws_name)
                        self._data_workspaces.add(ws_name, loaded_ws=ws)
                    else:
                        logger.warning(
                            f"Invalid number of spectra in workspace {ws_name}. Skipping loading of file.")
                except RuntimeError as e:
                    logger.error(
                        f"Failed to load file: {filename}. Error: {e}. \n Continuing loading of other files.")
            else:
                logger.warning(f"File {ws_name} has already been loaded")
        self.update_log_workspace_group()

    def create_log_workspace_group(self):
        # run information table
        run_info = self.make_runinfo_table()
        self._log_workspaces = GroupWorkspaces([run_info], OutputWorkspace='logs')
        # a table per logs
        logs = get_setting(output_settings.INTERFACES_SETTINGS_GROUP, output_settings.ENGINEERING_PREFIX, "logs")
        if logs:
            self._log_names = logs.split(',')
            for log in self._log_names:
                self.make_log_table(log)
                self._log_workspaces.add(log)

    def make_log_table(self, log):
        ws_log = CreateEmptyTableWorkspace(OutputWorkspace=log)
        ws_log.addColumn(type="float", name="avg")
        ws_log.addColumn(type="float", name="stdev")
        return ws_log

    def make_runinfo_table(self):
        run_info = CreateEmptyTableWorkspace()
        run_info.addColumn(type="str", name="Instrument")
        run_info.addColumn(type="int", name="Run")
        run_info.addColumn(type="str", name="Bank")
        run_info.addColumn(type="float", name="uAmps")
        run_info.addColumn(type="str", name="Title")
        return run_info

    def update_log_workspace_group(self):
        # both ws and name needed in event a ws is renamed and ws.name() is no longer correct

        if not self._data_workspaces:
            self.delete_logs()
            return

        if not self._log_workspaces:
            self.create_log_workspace_group()
        else:
            for log in self._log_names:
                if not ADS.doesExist(log):
                    self.make_log_table(log)
                    self._log_workspaces.add(log)
            if not ADS.doesExist("run_info"):
                self.make_runinfo_table()
                self._log_workspaces.add("run_info")
        # update log tables
        self.remove_all_log_rows()
        for irow, (ws_name, ws) in enumerate(self._data_workspaces.get_loaded_ws_dict().items()):
            self.add_log_to_table(ws_name, ws, irow)

    def add_log_to_table(self, ws_name, ws, irow):
        # both ws and name needed in event a ws is renamed and ws.name() is no longer correct
        # make dict for run if doesn't exist
        if ws_name not in self._log_values:
            self._log_values[ws_name] = dict()
        # add run info
        run = ws.getRun()
        row = [ws.getInstrument().getFullName(), ws.getRunNumber(), run.getProperty('bankid').value,
               run.getProtonCharge(), ws.getTitle()]
        self.write_table_row(ADS.retrieve("run_info"), row, irow)
        # add log data - loop over existing log workspaces not logs in settings as these might have changed
        currentRunLogs = [l.name for l in run.getLogData()]
        nullLogValue = full(2, nan)  # default nan if can't read/average log data
        if run.getProtonCharge() > 0 and "proton_charge" in currentRunLogs:
            for log in self._log_names:
                if log in self._log_values[ws_name]:
                    avg, stdev = self._log_values[ws_name][log]  # already averaged
                elif log in currentRunLogs:
                    avg, stdev = AverageLogData(ws_name, LogName=log, FixZero=False)
                else:
                    avg, stdev = nullLogValue
                self._log_values[ws_name][log] = [avg, stdev]  # update model dict (even if nan)
        else:
            self._log_values[ws_name] = {log: nullLogValue for log in self._log_names}
            logger.warning(f"{ws.name()} does not contain a proton charge log - log values cannot be averaged.")

        # write log values to table (nan if log could not be averaged)
        for log, avg_and_stdev in self._log_values[ws_name].items():
            self.write_table_row(ADS.retrieve(log), avg_and_stdev, irow)
        self.update_log_group_name()

    def remove_log_rows(self, row_numbers):
        DeleteTableRows(TableWorkspace=self._log_workspaces, Rows=list(row_numbers))
        self.update_log_group_name()

    def remove_all_log_rows(self):
        for ws in self._log_workspaces:
            ws.setRowCount(0)

    def delete_logs(self):
        if self._log_workspaces:
            ws_name = self._log_workspaces.name()
            self._log_workspaces = None
            DeleteWorkspace(ws_name)

    def update_log_group_name(self):
        run_info = ADS.retrieve('run_info')
        if run_info.rowCount() > 0:
            runs = run_info.column('Run')
            name = f"{run_info.row(0)['Instrument']}_{min(runs)}-{max(runs)}_logs"
            if not name == self._log_workspaces.name():
                RenameWorkspace(InputWorkspace=self._log_workspaces.name(), OutputWorkspace=name)
        else:
            self.delete_logs()

    def get_loaded_ws_list(self):
        return list(self._data_workspaces.get_loaded_ws_dict().keys())

    def get_active_ws_name_list(self):
        return self._data_workspaces.get_active_ws_name_list()

    def get_active_ws(self, loaded_ws_name):
        return self._data_workspaces[loaded_ws_name].get_active_ws()

    def get_active_ws_name(self, loaded_ws_name):
        return self._data_workspaces.get_active_ws_name(loaded_ws_name)

    def get_active_ws_sorted_by_primary_log(self):
        active_ws_dict = self._data_workspaces.get_active_ws_dict()
        tof_ws_inds = [ind for ind, ws in enumerate(active_ws_dict.values()) if
                       ws.getAxis(0).getUnit().caption() == 'Time-of-flight']
        primary_log = get_setting(output_settings.INTERFACES_SETTINGS_GROUP, output_settings.ENGINEERING_PREFIX,
                                  "primary_log")
        sort_ascending = get_setting(output_settings.INTERFACES_SETTINGS_GROUP, output_settings.ENGINEERING_PREFIX,
                                     "sort_ascending")
        ws_name_list = list(active_ws_dict.keys())
        if primary_log:
            log_table = ADS.retrieve(primary_log)
            isort = argsort(array(log_table.column('avg')))
            ws_list_tof = [ws_name_list[iws] for iws in isort if iws in tof_ws_inds]
        else:
            ws_list_tof = ws_name_list
        if sort_ascending == 'false':
            # settings can only be saved as text
            ws_list_tof = ws_list_tof[::-1]
        return ws_list_tof

    def update_fit(self, fit_props):
        for fit_prop in fit_props:
            wsname = fit_prop['properties']['InputWorkspace']
            self._fit_results[wsname] = {'model': fit_prop['properties']['Function'],
                                         'status': fit_prop['status']}
            self._fit_results[wsname]['results'] = defaultdict(list)  # {function_param: [[Y1, E1], [Y2,E2],...] }
            fnames = [x.split('=')[-1] for x in findall('name=[^,]*', fit_prop['properties']['Function'])]
            # get num params for each function (first elem empty as str begins with 'name=')
            # need to remove ties and constraints which are enclosed in ()
            nparams = [s.count('=') for s in
                       sub(r'=\([^)]*\)', '', fit_prop['properties']['Function']).split('name=')[1:]]
            params_dict = ADS.retrieve(fit_prop['properties']['Output'] + '_Parameters').toDict()
            # loop over rows in output workspace to get value and error for each parameter
            istart = 0
            for ifunc, fname in enumerate(fnames):
                for iparam in range(0, nparams[ifunc]):
                    irow = istart + iparam
                    key = '_'.join([fname, params_dict['Name'][irow].split('.')[-1]])  # funcname_param
                    self._fit_results[wsname]['results'][key].append([
                        params_dict['Value'][irow], params_dict['Error'][irow]])
                    if key in fit_prop['peak_centre_params']:
                        # param corresponds to a peak centre in TOF which we also need in dspacing
                        # add another entry into the results dictionary
                        key_d = key + "_dSpacing"
                        try:
                            dcen = self._convert_TOF_to_d(params_dict['Value'][irow], wsname)
                            dcen_er = self._convert_TOFerror_to_derror(params_dict['Error'][irow], dcen, wsname)
                            self._fit_results[wsname]['results'][key_d].append([dcen, dcen_er])
                        except (ValueError, RuntimeError) as e:
                            logger.warning(f"Unable to output {key_d} parameters for TOF={params_dict['Value'][irow]}: " + str(e))
                istart += nparams[ifunc]
            # append the cost function value (in this case always chisq/DOF) as don't let user change cost func
            # always last row in parameters table
            self._fit_results[wsname]['costFunction'] = params_dict['Value'][-1]
        self.create_fit_tables()

    def create_fit_tables(self):
        wslist = []  # ws to be grouped
        # extract fit parameters and errors
        nruns = len(self.get_loaded_ws_list())  # num of rows of output workspace
        # get unique set of function parameters across all workspaces
        func_params = set(chain(*[list(d['results'].keys()) for d in self._fit_results.values()]))
        for param in func_params:
            # get max number of repeated func in a model (num columns of output workspace)
            nfuncs = max([len(d['results'][param]) for d in self._fit_results.values() if param in d['results']])
            # make output workspace
            ipeak = list(range(1, nfuncs + 1)) * nruns
            ws = CreateWorkspace(OutputWorkspace=param, DataX=ipeak, DataY=ipeak, NSpec=nruns)
            # axis for labels in workspace
            axis = TextAxis.create(nruns)
            for iws, wsname in enumerate(self.get_active_ws_name_list()):
                if wsname in self._fit_results and param in self._fit_results[wsname]['results']:
                    fitvals = array(self._fit_results[wsname]['results'][param])
                    data = vstack((fitvals, full((nfuncs - fitvals.shape[0], 2), nan)))
                else:
                    data = full((nfuncs, 2), nan)
                ws.setY(iws, data[:, 0])
                ws.setE(iws, data[:, 1])
                # label row
                axis.setLabel(iws, wsname)
            ws.replaceAxis(1, axis)
            wslist += [ws]
        # table for model summary/info
        model = CreateEmptyTableWorkspace(OutputWorkspace='model')
        model.addColumn(type="str", name="Workspace")
        model.addColumn(type="float", name="chisq/DOF")  # always is for LM minimiser (users can't change)
        model.addColumn(type="str", name="status")
        model.addColumn(type="str", name="Model")
        for iws, wsname in enumerate(self.get_active_ws_name_list()):
            if wsname in self._fit_results:
                row = [wsname, self._fit_results[wsname]['costFunction'],
                       self._fit_results[wsname]['status'], self._fit_results[wsname]['model']]
                self.write_table_row(model, row, iws)
            else:
                self.write_table_row(model, ['', nan, ''], iws)
        wslist += [model]
        group_name = self._log_workspaces.name().split('_log')[0] + '_fits'
        self._fit_workspaces = GroupWorkspaces(wslist, OutputWorkspace=group_name)

    # handle ADS remove. name workspace has already been deleted
    def remove_workspace(self, name):
        ws_loaded = self._data_workspaces.get(name, None)
        if ws_loaded:
            bgsub_ws_name=self._data_workspaces[name].bgsub_ws_name
            removed = self._data_workspaces.pop(name).loaded_ws
            # deleting bg sub workspace will generate further remove_workspace event so ensure this is done after
            # removing record from _data_workspaces to avoid circular call
            if bgsub_ws_name:
                DeleteWorkspace(bgsub_ws_name)
            self.update_log_workspace_group()
            return removed
        else:
            ws_loaded_name = self._data_workspaces.get_loaded_workspace_name_from_bgsub(name)
            if ws_loaded_name:
                removed = self._data_workspaces[ws_loaded_name].bgsub_ws
                self._data_workspaces[ws_loaded_name].bgsub_ws = None
                self._data_workspaces[ws_loaded_name].bgsub_ws_name = None
                self._data_workspaces[ws_loaded_name].bg_params = []
                return removed

    def replace_workspace(self, name, workspace):
        self._data_workspaces.replace_workspace(name, workspace)

    def update_workspace_name(self, old_name, new_name):
        if new_name not in self.get_all_workspace_names():
            self._data_workspaces.rename(old_name, new_name)
            if old_name in self._log_values:
                self._log_values[new_name] = self._log_values.pop(old_name)
        else:
            logger.warning(f"There already exists a workspace with name {new_name}.")
        self.update_log_workspace_group()

    # handle ADS clear
    def clear_workspaces(self):
        self._data_workspaces.clear()
        self.set_log_workspaces_none()

    def delete_workspaces(self):
        if self._log_workspaces:
            ws_name = self._log_workspaces.name()
            self._log_workspaces = None
            DeleteWorkspace(ws_name)
        removed_ws_list = []
        for ws_name in self._data_workspaces.get_loaded_workpace_names():
            removed_ws_list.extend(self.delete_workspace(ws_name))
        return removed_ws_list

    def delete_workspace(self, loaded_ws_name):
        removed = self._data_workspaces.pop(loaded_ws_name)
        removed_ws_list = [removed.loaded_ws]
        DeleteWorkspace(removed.loaded_ws)
        if removed.bgsub_ws:
            DeleteWorkspace(removed.bgsub_ws)
            removed_ws_list.append(removed.bgsub_ws)
        self.update_log_workspace_group()
        return removed_ws_list

    def get_loaded_workspaces(self):
        return self._data_workspaces.get_loaded_ws_dict()

    def get_all_workspace_names(self):
        return self._data_workspaces.get_loaded_workpace_names() + self._data_workspaces.get_bgsub_workpace_names()

    def get_log_workspaces_name(self):
        return [ws.name() for ws in self._log_workspaces] if self._log_workspaces else ''

    def get_bgsub_workspaces(self):
        return self._data_workspaces.get_bgsub_ws_dict()

    def get_bgsub_workspace_names(self):
        return self._data_workspaces.get_bgsub_ws_name_dict()

    def get_bg_params(self):
        return self._data_workspaces.get_bg_params_dict()

    def get_fit_results(self):
        return self._fit_results

    def create_or_update_bgsub_ws(self, ws_name, bg_params):
        ws = self._data_workspaces[ws_name].loaded_ws
        ws_bg = self._data_workspaces[ws_name].bgsub_ws
        ws_bg_params = self._data_workspaces[ws_name].bg_params
        if not ws_bg or ws_bg_params == [] or bg_params[1:] != ws_bg_params[1:]:
            background = self.estimate_background(ws_name, *bg_params[1:])
            self._data_workspaces[ws_name].bg_params = bg_params
            bgsub_ws_name = ws_name + "_bgsub"
            bgsub_ws = Minus(LHSWorkspace=ws, RHSWorkspace=background, OutputWorkspace=bgsub_ws_name)
            self._data_workspaces[ws_name].bgsub_ws = bgsub_ws
            self._data_workspaces[ws_name].bgsub_ws_name = bgsub_ws_name
            DeleteWorkspace(background)
        else:
            logger.notice("Background workspace already calculated")

    def update_bgsub_status(self, ws_name, status):
        if self._data_workspaces[ws_name].bg_params:
            self._data_workspaces[ws_name].bg_params[0] = status

    def estimate_background(self, ws_name, niter, xwindow, doSGfilter):
        try:
            ws_bg = EnggEstimateFocussedBackground(InputWorkspace=ws_name, OutputWorkspace=ws_name + "_bg",
                                                   NIterations=niter, XWindow=xwindow, ApplyFilterSG=doSGfilter)
        except (ValueError, RuntimeError) as e:
            # ValueError when Niter not positive integer, RuntimeError when Window too small
            logger.error("Error on arguments supplied to EnggEstimateFocusedBackground: " + str(e))
            ws_bg = SetUncertainties(InputWorkspace=ws_name)  # copy data and zero errors
            ws_bg = Minus(LHSWorkspace=ws_bg, RHSWorkspace=ws_bg)  # workspace of zeros with same num spectra
        return ws_bg

    def plot_background_figure(self, ws_name):
        def on_draw(event):
            axes = event.canvas.figure.get_axes()
            labels = [line.get_label() for line in axes[0].get_lines()]
            ibg = labels.index('background')
            idata = int(not bool(ibg))
            bg_line = axes[0].get_lines()[ibg]
            data_line = axes[0].get_lines()[idata]
            bgsub_line = axes[1].get_lines()[0]
            bg_line.set_ydata(data_line.get_ydata() - bgsub_line.get_ydata())
            event.canvas.draw_idle()
            self._mpl_bg_fig_signal = event.canvas.mpl_connect("draw_event", on_draw)

        ws = self._data_workspaces[ws_name].loaded_ws
        ws_bgsub = self._data_workspaces[ws_name].bgsub_ws
        if ws_bgsub:
            fig, ax = subplots(2, 1, sharex=True, gridspec_kw={'height_ratios': [2, 1]},
                               subplot_kw={'projection': 'mantid'})
            bg = Minus(LHSWorkspace=ws_name, RHSWorkspace=ws_bgsub, StoreInADS=False)
            ax[0].plot(ws, 'x')
            ax[1].plot(ws_bgsub, 'x', label='background subtracted data')
            ax[0].plot(bg, '-r', label='background')
            ax[0].legend(fontsize=8.0).set_draggable(True).legend
            ax[1].legend(fontsize=8.0).set_draggable(True).legend
            fig.canvas.mpl_connect("draw_event", on_draw)
            fig.show()

    def get_last_added(self):
        return self._last_added

    def get_sample_log_from_ws(self, ws_name, log_name):
        return self._data_workspaces[ws_name].loaded_ws.getSampleDetails().getLogData(log_name).value

    def set_log_workspaces_none(self):
        # to be used in the event of Ads clear, as trying to reference the deleted grp ws results in an error
        self._log_workspaces = None

    def _convert_TOF_to_d(self, tof, ws_name):
        diff_consts = self._get_diff_constants(ws_name)
        return UnitConversion.run("TOF", "dSpacing", tof, 0, DeltaEModeType.Elastic, diff_consts)  # L1=0 (ignored)

    def _convert_TOFerror_to_derror(self, tof_error, d, ws_name):
        diff_consts = self._get_diff_constants(ws_name)
        difc = diff_consts[UnitParams.difc]
        difa = diff_consts[UnitParams.difa] if UnitParams.difa in diff_consts else 0
        return tof_error / (2 * difa * d + difc)

    def _get_diff_constants(self, ws_name):
        """
        Get diffractometer constants from workspace
        TOF = difc*d + difa*(d^2) + tzero
        """
        ws = ADS.retrieve(ws_name)
        si = ws.spectrumInfo()
        diff_consts = si.diffractometerConstants(0)  # output is a UnitParametersMap
        return diff_consts

    @staticmethod
    def write_table_row(ws_table, row, irow):
        if irow > ws_table.rowCount() - 1:
            ws_table.setRowCount(irow + 1)
        [ws_table.setCell(irow, icol, row[icol]) for icol in range(0, len(row))]

    @staticmethod
    def _generate_workspace_name(filepath):
        wsname = path.splitext(path.split(filepath)[1])[0]
        return wsname
