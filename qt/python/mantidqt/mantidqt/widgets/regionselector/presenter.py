# Mantid Repository : https://github.com/mantidproject/mantid#
# Copyright &copy; 2021 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
#  This file is part of the mantid workbench.
from distutils.version import LooseVersion
from typing import Callable

from .view import RegionSelectorView
from ..observers.observing_presenter import ObservingPresenter
from ..sliceviewer.models.dimensions import Dimensions
from ..sliceviewer.models.workspaceinfo import WorkspaceInfo, WS_TYPE
from ..sliceviewer.presenters.base_presenter import SliceViewerBasePresenter
from mantid.api import RegionSelectorObserver

# 3rd party imports
import matplotlib
from matplotlib.widgets import RectangleSelector


class Selector(RectangleSelector):
    active_handle_alpha = 0.5
    kwargs = {"useblit": False,  # rectangle persists on button release
              "button": [1],
              "minspanx": 5,
              "minspany": 5,
              "spancoords": "pixels",
              "interactive": True}

    def __init__(self, region_type: str, color: str, *args):
        if LooseVersion(matplotlib.__version__) >= LooseVersion("3.5.0"):
            self.kwargs["props"] = dict(facecolor="white", edgecolor=color, alpha=0.2, linewidth=2, fill=True)
            self.kwargs["drag_from_anywhere"] = True
            self.kwargs["ignore_event_outside"] = True

        super().__init__(*args, **self.kwargs)
        self._region_type = region_type

    def region_type(self):
        return self._region_type

    def set_active(self, active: bool) -> None:
        """Hide the handles of a selector if it is not active."""
        self.set_handle_props(alpha=self.active_handle_alpha if active else 0)
        super().set_active(active)


class RegionSelector(ObservingPresenter, SliceViewerBasePresenter):
    def __init__(self, ws=None, parent=None, view=None):
        if ws and WorkspaceInfo.get_ws_type(ws) != WS_TYPE.MATRIX:
            raise NotImplementedError("Only Matrix Workspaces are currently supported by the region selector.")

        self.notifyee = None
        self.view = view if view else RegionSelectorView(self, parent)
        super().__init__(ws, self.view._data_view)
        self._selectors: list[Selector] = []
        self._drawing_region = False

        if ws:
            self._initialise_dimensions(ws)
            self._set_workspace(ws)

    def subscribe(self, notifyee: RegionSelectorObserver):
        self.notifyee = notifyee

    def dimensions_changed(self) -> None:
        self.new_plot()

    def slicepoint_changed(self) -> None:
        pass

    def canvas_clicked(self, event) -> None:
        if self._drawing_region:
            return

        for selector in self._selectors:
            selector.set_active(False)

        clicked_selector = self._find_selector_if(lambda x: self._contains_point(x.extents, event.xdata, event.ydata))
        if clicked_selector is not None:
            # Ensure only one selector is active to avoid confusing matplotlib behaviour
            clicked_selector.set_active(True)

    def key_pressed(self, event) -> None:
        """Handles key press events."""
        if event.key == "delete":
            selector = self._find_selector_if(lambda x: x.active)
            if selector is not None:
                self._remove_selector(selector)

    def _remove_selector(self, selector: Selector) -> None:
        """Remove selector from the plot."""
        selector.set_active(False)
        for artist in selector.artists:
            artist.set_visible(False)
        selector.update()
        self._selectors.remove(selector)

    def _find_selector_if(self, predicate: Callable) -> Selector:
        """Find the first selector which agrees with a predicate. Return None if no selector is found."""
        for selector in self._selectors:
            if predicate(selector):
                return selector
        return None

    def zoom_pan_clicked(self, active) -> None:
        pass

    def new_plot(self, *args, **kwargs):
        if self.model.ws:
            self.new_plot_matrix()

    def nonorthogonal_axes(self, state: bool) -> None:
        pass

    def update_workspace(self, workspace) -> None:
        if WorkspaceInfo.get_ws_type(workspace) != WS_TYPE.MATRIX:
            raise NotImplementedError("Only Matrix Workspaces are currently supported by the region selector.")

        if not self.model.ws:
            self._initialise_dimensions(workspace)

        self._set_workspace(workspace)

    def cancel_drawing_region(self):
        """
        Cancel drawing a region if a different toolbar option is pressed.
        """
        if self._drawing_region:
            self._selectors.pop()
            self._drawing_region = False

    def add_rectangular_region(self, region_type, color):
        """
        Add a rectangular region selection tool.
        """
        for selector in self._selectors:
            selector.set_active(False)

        self._selectors.append(Selector(region_type, color, self.view._data_view.ax, self._on_rectangle_selected))

        self._drawing_region = True

    def get_region(self, region_type):
        # extents contains x1, x2, y1, y2. Just store y (spectra) for now
        result = []
        for selector in self._selectors:
            if selector.region_type() == region_type:
                result.extend([selector.extents[2], selector.extents[3]])
        return result

    def _initialise_dimensions(self, workspace):
        self.view.create_dimensions(dims_info=Dimensions.get_dimensions_info(workspace))
        self.view.create_axes_orthogonal(
            redraw_on_zoom=not WorkspaceInfo.can_support_dynamic_rebinning(workspace))

    def _set_workspace(self, workspace):
        self.model.ws = workspace
        self.view.set_workspace(workspace)
        self.new_plot()

    def _on_rectangle_selected(self, eclick, erelease):
        """
        Callback when a rectangle has been draw on the axes
        :param eclick: Event marking where the mouse was clicked
        :param erelease: Event marking where the mouse was released
        """
        self._drawing_region = False

        if self.notifyee:
            self.notifyee.notifyRegionChanged()

    @staticmethod
    def _contains_point(extents, x, y):
        return extents[0] <= x <= extents[1] and extents[2] <= y <= extents[3]
