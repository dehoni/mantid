from mantid.api import PythonAlgorithm, AlgorithmFactory, MatrixWorkspaceProperty, WorkspaceUnitValidator, \
                       InstrumentValidator, FileProperty, FileAction, mtd
from mantid.kernel import Direction, CompositeValidator
from mantid.dataobjects import Workspace2D

import ruamel.yaml
from ruamel.yaml.comments import CommentedMap, CommentedSeq

import math


class SaveYDA(PythonAlgorithm):
    """ Save data in yaml/frida 2.0 format from a Workspace2D.
    """

    def category(self):
        """Return category
        """
        return "DataHandling\\Text"

    def name(self):
        """Return name
        """
        return "SaveYDA"

    def summary(self):
        """Return summary
        """
        return "Save Workspace to a Frida 2.0 yaml format"

    def PyInit(self):
        """Declare properties
        """
        wsValidators = CompositeValidator()
        # X axis must be a NumericAxis in energy transfer units.
        wsValidators.add(WorkspaceUnitValidator("DeltaE"))
        # Workspace must have an Instrument
        wsValidators.add(InstrumentValidator())

        self.declareProperty(MatrixWorkspaceProperty(name="InputWorkspace", defaultValue="", direction=Direction.Input,
                             validator=wsValidators), doc="Workspace name for input")
        self.declareProperty(FileProperty(name="Filename", defaultValue="", action=FileAction.Save, extensions=""),
                             doc="The name to use when writing the file")

    def validateInputs(self):
        """Basic validation for inputs.
        """
        issues = dict()
        # Only MomentumTransfer is allowed
        allowUn = "MomentumTransfer"
        ws = self.getProperty("InputWorkspace").value
        # Y axis must be either a SpectrumAxis or a NumericAxis in q units.
        # workspace must be a Workspace2D
        if ws:
            ax = ws.getAxis(1)

            if not ax.isSpectra() and ax.getUnit().unitID() != allowUn:
                issues["InputWorkspace"] = "Y axis is not 'Spectrum Axis' or 'Momentum Transfer'"

            if not isinstance(ws, Workspace2D):
                issues["InputWorkspace"] = "Input Workspace is not a Workspace2D"

        return issues

    def PyExec(self):
        """ Main execution body
        """
        # Properties
        ws = mtd[self.getPropertyValue("InputWorkspace")]
        filename = self.getProperty("Filename").value

        # check workspace exists
        if not ws:
            raise NotImplementedError("InputWorkspace does not exist")

        run = ws.getRun()
        ax = ws.getAxis(1)
        nHist = ws.getNumberHistograms()

        # check sample logs exists
        if len(run.getLogData()) == 0:
            raise NotImplementedError("No sample log data exist in workspace: "
                                      + self.getPropertyValue("InputWorkspace"))

        # save sample log data in lists, commented sequences an commented maps
        # commented sequences and maps are used to keep Data in the order they get inserted
        # if a log does not exist a warning is written on the log and the data is not saved in the file

        metadata = CommentedMap()

        metadata["format"] = "yaml/frida 2.0"
        metadata["type"] = "generic tabular data"

        hist = []

        if run.hasProperty("proposal_number"):
            propn = "Proposal number " + run.getLogData("proposal_number").value
            hist.append(propn)
        else:
            self.log().warning("no proposal number found")

        if run.hasProperty("proposal_title"):
            propt = run.getLogData("proposal_title").value
            hist.append(propt)
        else:
            self.log().warning("no proposal title found")

        if run.hasProperty("experiment_team"):
            expt = run.getLogData("experiment_team").value
            hist.append(expt)
        else:
            self.log().warning("no experiment team found")

        hist.append("data reduced with mantid")

        rpar = []

        if run.hasProperty("temperature"):
            temperature = float(run.getLogData("temperature").value)

            temp = CommentedMap()
            temp["name"] = "T"
            temp["unit"] = "K"
            temp["val"] = temperature
            temp["stdv"] = 0

            rpar.append(temp)
        else:
            self.log().warning("no temperature found")

        if run.hasProperty("Ei"):
            eimeV = float(run.getLogData("Ei").value)

            ei = CommentedMap()
            ei["name"] = "Ei"
            ei["unit"] = "meV"
            ei["val"] = eimeV
            ei["stdv"] = 0

            rpar.append(ei)
        else:
            self.log().warning("no Ei found")

        coord = CommentedMap()

        x = CommentedMap()

        x["name"] = "w"
        x["unit"] = "meV"

        # set_flow style is used to keep the Frida 2.0 yaml format
        coord["x"] = x
        coord["x"].fa.set_flow_style()

        y = CommentedMap()

        y["name"] = "S(q,w)"
        y["unit"] = "meV-1"

        coord["y"] = y
        coord["y"].fa.set_flow_style()

        z = CommentedMap()

        if ax.isSpectra():
            zname = "2th"
            zunit = "deg"
        else:
            zname = "q"
            zunit = "A-1"

        z["name"] = zname
        z["unit"] = zunit

        coord["z"] = z
        coord["z"].fa.set_flow_style()

        slices = []

        bin = []

        # if y axis is SpectrumAxis
        if ax.isSpectra:
            samplePos = ws.getInstrument().getSample().getPos()
            sourcePos = ws.getInstrument().getSource().getPos()
            beamPos = samplePos - sourcePos
            for i in range(nHist):
                detector = ws.getDetector(i)
                # convert radians to degrees
                twoTheta = detector.getTwoTheta(samplePos, beamPos)*180/math.pi
                bin.append(twoTheta)
        elif ax.length() == nHist:
            # if y axis contains bin centers
            for i in range(ax.length()):
                bin.append(ax.getValue())
        else:
            # get the bin centers not the bin edges
            bin = self._get_bin_centers(ax)

        for i in range(nHist):

            slicethis = CommentedMap()

            # add j to slices, j = counts
            slicethis["j"] = i

            # save in list and commented Map to keep format
            val = CommentedMap()
            val["val"] = bin[i]
            value = [val]
            # z is bin centers of y axis, SpectrumAxis or NumericAxis in q units
            slicethis["z"] = CommentedSeq(value)
            slicethis["z"].fa.set_flow_style()

            xax = ws.readX(i)
            # get the bin centers not the bin edges
            xcenters = self._get_bin_centers(xax)
            # x axis is NumericAxis in energy transfer units
            xx = [float(j) for j in xcenters]
            slicethis["x"] = CommentedSeq(xx)
            slicethis["x"].fa.set_flow_style()

            ys = ws.dataY(i)
            # y is dataY of the workspace
            yy = [float(j) for j in ys]
            slicethis["y"] = CommentedSeq(yy)
            slicethis["y"].fa.set_flow_style()

            slices.append(slicethis)

        data = CommentedMap()

        data["Meta"] = metadata
        data["History"] = hist
        data["Coord"] = coord
        data["RPar"] = rpar
        data["Slices"] = slices
        data["Slices"] = slices

        # create yaml file
        try:
            with open(filename, "w") as outfile:
                ruamel.yaml.round_trip_dump(data, outfile, block_seq_indent=2, indent=4)
                outfile.close()
        except:
            raise RuntimeError("Can't write in File" + filename)

    def _get_bin_centers(self, ax):
        """ calculates the bin centers from the bin edges
        :param bin center axis
        :return list of bin centers:
        """
        bin = []

        for i in range(1, ax.size):
                bin.append((ax[i]+ax[i-1])/2)

        return bin

#---------------------------------------------------------------------------------------------------------------------#

AlgorithmFactory.subscribe(SaveYDA)
