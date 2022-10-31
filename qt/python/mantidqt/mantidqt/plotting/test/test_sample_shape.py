# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2022 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
#  This file is part of the mantid workbench.
#
#

from numpy.testing import assert_array_equal, assert_allclose
from unittest import TestCase, main
from unittest.mock import patch, Mock
import numpy as np
import matplotlib
matplotlib.use('AGG')  # noqa

import mantid
from mantid.api import AnalysisDataService as ADS
from mantid.simpleapi import (CreateWorkspace, CreateSampleWorkspace, SetSample, LoadSampleShape, DeleteWorkspace,
                              LoadInstrument, SetUB)
from mantidqt.plotting import sample_shape

workspace_name = "ws_shape"
test_files_path = mantid.config.getString('defaultsave.directory')


def setup_workspace_shape_from_CSG_merged():
    CreateWorkspace(OutputWorkspace="ws_shape", DataX=[1, 1], DataY=[2, 2])
    merge_xml = ' \
    <cylinder id="stick"> \
    <centre-of-bottom-base x="-0.5" y="0.0" z="0.0" /> \
    <axis x="1.0" y="0.0" z="0.0" />  \
    <radius val="0.05" /> \
    <height val="1.0" /> \
    </cylinder> \
    \
    <sphere id="some-sphere"> \
    <centre x="0.7"  y="0.0" z="0.0" /> \
    <radius val="0.2" /> \
    </sphere> \
    \
    <rotate-all x="90" y="-45" z="0" /> \
    <algebra val="some-sphere (: stick)" /> \
    '
    SetSample("ws_shape", Geometry={'Shape': 'CSG', 'Value': merge_xml})
    workspace = ADS.retrieve(workspace_name)
    return workspace


def setup_workspace_sample_and_container_CSG():
    CreateWorkspace(OutputWorkspace=workspace_name, DataX=[1, 1], DataY=[2, 2])
    SetSample(workspace_name,
              Geometry={'Shape': 'Cylinder', 'Height': 4.0,
                        'Radius': 2.0, 'Center': [0., 0., 0.]},
              Material={'ChemicalFormula': '(Li7)2-C-H4-N-Cl6',
                        'NumberDensity': 0.1},
              ContainerGeometry={'Shape': 'HollowCylinder', 'Height': 4.0,
                                 'InnerRadius': 2.0, 'OuterRadius': 2.3,
                                 'Center': [0., 0., 0.]},
              ContainerMaterial={'ChemicalFormula': 'Al',
                                 'NumberDensity': 0.01})
    return ADS.retrieve(workspace_name)


def setup_workspace_container_CSG():
    CreateWorkspace(OutputWorkspace=workspace_name, DataX=[1, 1], DataY=[2, 2])

    SetSample(workspace_name,
              ContainerGeometry={'Shape': 'HollowCylinder', 'Height': 4.0,
                                 'InnerRadius': 2.0, 'OuterRadius': 2.3,
                                 'Center': [0., 0., 0.]},
              ContainerMaterial={'ChemicalFormula': 'Al',
                                 'NumberDensity': 0.01})
    return ADS.retrieve(workspace_name)


def setup_workspace_shape_from_mesh():
    CreateSampleWorkspace(OutputWorkspace="ws_shape")
    LoadSampleShape(InputWorkspace="ws_shape", OutputWorkspace="ws_shape", Filename="tube.stl")
    return ADS.retrieve("ws_shape")


def setup_workspace_sample_container_and_components_from_mesh():
    workspace = CreateWorkspace(OutputWorkspace="ws_shape", DataX=[1, 1], DataY=[2, 2])
    LoadInstrument(Workspace=workspace, RewriteSpectraMap=True, InstrumentName="Pearl")
    SetSample(workspace, Environment={'Name': 'Pearl'})
    return workspace


class PlotSampleShapeTest(TestCase):

    @classmethod
    def setUpClass(cls) -> None:
        cls.RELATIVE_TOLERANCE = 0.001

    def tearDown(self) -> None:
        if "ws_shape" in ADS:
            DeleteWorkspace(workspace_name)

    def test_CSG_merged_shape_is_valid(self):
        workspace = setup_workspace_shape_from_CSG_merged()
        self.assertTrue(workspace.sample().getShape())
        self.assertTrue(sample_shape.get_valid_sample_shape_from_workspace(workspace))

    def test_CSG_sphere_is_valid(self):
        workspace = CreateSampleWorkspace(OutputWorkspace="ws_shape")
        self.assertTrue(workspace.sample().getShape())
        self.assertTrue(sample_shape.get_valid_sample_shape_from_workspace(workspace))

    def test_CSG_empty_shape_is_not_valid(self):
        workspace = CreateWorkspace(OutputWorkspace="ws_shape", DataX=[1, 1], DataY=[2, 2])
        self.assertTrue(workspace.sample().getShape())
        self.assertFalse(sample_shape.get_valid_sample_shape_from_workspace(workspace))

    def test_mesh_is_valid(self):
        workspace = setup_workspace_shape_from_mesh()
        self.assertTrue(workspace.sample().getShape())
        self.assertTrue(sample_shape.get_valid_sample_shape_from_workspace(workspace))

    def test_container_valid(self):
        workspace = setup_workspace_container_CSG()
        self.assertTrue(sample_shape.get_valid_container_shape_from_workspace(workspace))

    @patch("mantidqt.plotting.sample_shape.is_mesh_not_empty")
    def test_container_invalid(self, mock_is_mesh_not_empty):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        mock_is_mesh_not_empty.return_value = False
        self.assertFalse(sample_shape.get_valid_container_shape_from_workspace(workspace))

    def test_component_valid(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        self.assertTrue(sample_shape.get_valid_component_shape_from_workspace(workspace, 1))

    @patch("mantidqt.plotting.sample_shape.is_mesh_not_empty")
    def test_component_invalid(self, mock_is_mesh_not_empty):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        mock_is_mesh_not_empty.return_value = False
        self.assertFalse(sample_shape.get_valid_container_shape_from_workspace(workspace))

    def test_plot_created_for_CSG_sphere_sample_only(self):
        CreateSampleWorkspace(OutputWorkspace="ws_shape")
        shape_plot = sample_shape.plot_sample_container_and_components("ws_shape")
        self.assertTrue(shape_plot)

    def test_plot_created_for_CSG_merged_sample_only(self):
        workspace = setup_workspace_shape_from_CSG_merged()
        shape_plot = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(shape_plot)

    def test_plot_created_for_mesh_sample_only(self):
        workspace = setup_workspace_shape_from_mesh()
        shape_plot_axes = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(shape_plot_axes)

    def test_plot_created_for_mesh_container_and_components(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        shape_plot_figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(shape_plot_figure)
        container_added_to_plot = sample_shape.plot_container(workspace, shape_plot_figure)
        components_added_to_plot = sample_shape.plot_components(workspace, shape_plot_figure)
        self.assertTrue(container_added_to_plot)
        self.assertTrue(components_added_to_plot)

    @patch("mantidqt.plotting.sample_shape.set_perspective")
    @patch("mantidqt.plotting.sample_shape.set_axes_labels")
    @patch("mantidqt.plotting.sample_shape.show_the_figure")
    @patch("mantidqt.plotting.sample_shape.plot_container")
    @patch("mantidqt.plotting.sample_shape.plot_components")
    def test_add_components_if_environment(self, mock_plot_container, mock_plot_components,
                                           mock_show_the_figure, mock_set_axes_labels,
                                           mock_set_perspective):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertEqual(1, mock_plot_container.call_count)
        self.assertEqual(1, mock_plot_components.call_count)
        self.assertEqual(1, mock_show_the_figure.call_count)
        self.assertEqual(1, mock_set_axes_labels.call_count)
        self.assertEqual(1, mock_set_perspective.call_count)

    @patch("mantidqt.plotting.sample_shape.set_perspective")
    @patch("mantidqt.plotting.sample_shape.set_axes_labels")
    @patch("mantidqt.plotting.sample_shape.show_the_figure")
    @patch("mantidqt.plotting.sample_shape.plot_container")
    @patch("mantidqt.plotting.sample_shape.plot_components")
    def test_do_not_add_components_if_no_environment(self, mock_plot_container, mock_plot_components,
                                                     mock_show_the_figure, mock_set_axes_labels,
                                                     mock_set_perspective):
        workspace = setup_workspace_shape_from_mesh()
        sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertEqual(0, mock_plot_container.call_count)
        self.assertEqual(0, mock_plot_components.call_count)
        self.assertEqual(1, mock_show_the_figure.call_count)
        self.assertEqual(1, mock_set_axes_labels.call_count)
        self.assertEqual(1, mock_set_perspective.call_count)

    @patch("mantidqt.plotting.sample_shape.set_perspective")
    @patch("mantidqt.plotting.sample_shape.set_axes_labels")
    @patch("mantidqt.plotting.sample_shape.show_the_figure")
    @patch("mantidqt.plotting.sample_shape.call_set_mesh_axes_equal")
    def test_call_axes_equal_once_for_sample_only(self, mock_call_set_mesh_axes_equal,
                                                  mock_show_the_figure, mock_set_axes_labels,
                                                  mock_set_perspective):
        workspace = setup_workspace_shape_from_mesh()
        sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertEqual(1, mock_call_set_mesh_axes_equal.call_count)
        self.assertEqual(1, mock_show_the_figure.call_count)
        self.assertEqual(1, mock_set_axes_labels.call_count)
        self.assertEqual(1, mock_set_perspective.call_count)

    @patch("mantidqt.plotting.sample_shape.set_perspective")
    @patch("mantidqt.plotting.sample_shape.set_axes_labels")
    @patch("mantidqt.plotting.sample_shape.show_the_figure")
    @patch("mantidqt.plotting.sample_shape.call_set_mesh_axes_equal")
    def test_call_axes_equal_once_for_sample_and_components(self, mock_call_set_mesh_axes_equal,
                                                            mock_show_the_figure, mock_set_axes_labels,
                                                            mock_set_perspective):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        shape_plot_figure = sample_shape.plot_sample_container_and_components(workspace.name())
        sample_shape.plot_container(workspace, shape_plot_figure)
        sample_shape.plot_components(workspace, shape_plot_figure)
        self.assertEqual(1, mock_call_set_mesh_axes_equal.call_count)
        self.assertEqual(1, mock_show_the_figure.call_count)
        self.assertEqual(1, mock_set_axes_labels.call_count)
        self.assertEqual(1, mock_set_perspective.call_count)

    def test_get_overall_limits_for_sample_only(self):
        workspace = setup_workspace_shape_from_mesh()
        minmax_xyz = sample_shape.overall_limits_for_all_meshes(workspace)
        assert_allclose([-0.1, -0.099993, -0.15, 0.1, 0.099993, 0.15], minmax_xyz, rtol=self.RELATIVE_TOLERANCE)

    def test_get_overall_limits_for_sample_only_PEARL(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        minmax_xyz = sample_shape.overall_limits_for_all_meshes(workspace, include_components=False)
        assert_allclose([-0.002934, -0.002939, -0.0018, 0.002934, 0.002939, 0.0018],
                        minmax_xyz, rtol=self.RELATIVE_TOLERANCE)

    def test_get_overall_limits_for_sample_and_components_PEARL(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        minmax_xyz = sample_shape.overall_limits_for_all_meshes(workspace)
        assert_allclose([-0.0359, -0.0359, -0.01481, 0.0359, 0.0359, 0.01482],
                        minmax_xyz, rtol=self.RELATIVE_TOLERANCE)

    def test_set_axes_to_largest_mesh(self):
        workspace = setup_workspace_shape_from_mesh()
        minmax_xyz = sample_shape.overall_limits_for_all_meshes(workspace)
        assert_allclose([-0.1, -0.099993, -0.15, 0.1, 0.099993, 0.15], minmax_xyz, rtol=self.RELATIVE_TOLERANCE)

    def test_overall_limits_for_every_axis(self):

        mesh_points = np.array([np.array([3, 5, 1]),
                                np.array([4, 7, 8]),
                                np.array([5, 6, 9]),
                                np.array([3, 4, 5])])
        self.assertEqual([3, 4, 1, 5, 7, 9], sample_shape.overall_limits_for_every_axis(mesh_points))

    def test_greater_limits(self):
        # [min_x, min_y, min_z, max_x, max_y, max_z]
        self.assertEqual([1, 2, 3, 10, 11, 12], sample_shape.greater_limits([1, 2, 3, 4, 5, 6], [7, 8, 9, 10, 11, 12]))
        self.assertEqual([1, 2, 3, 10, 11, 12], sample_shape.greater_limits([7, 2, 9, 4, 11, 6], [1, 8, 3, 10, 5, 12]))
        self.assertEqual([1, 2, 3, 10, 11, 12], sample_shape.greater_limits([1, 8, 3, 10, 5, 12], [7, 2, 9, 4, 11, 6]))
        self.assertEqual([1, 2, 3, 10, 11, 12], sample_shape.greater_limits([7, 8, 9, 10, 11, 12], [1, 2, 3, 4, 5, 6]))

    # Sample and Container
    def test_sample_valid_and_container_invalid(self):
        workspace = setup_workspace_shape_from_CSG_merged()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(f'Sample: {workspace.name()}', figure.gca().get_title())

    def test_sample_container_valid(self):
        workspace = setup_workspace_sample_and_container_CSG()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(f'Sample and Container: {workspace.name()}', figure.gca().get_title())

    def test_sample_invalid_container_valid(self):
        workspace = setup_workspace_container_CSG()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(f'Container: {workspace.name()}', figure.gca().get_title())

    def test_sample_and_container_invalid(self):
        workspace = CreateWorkspace(OutputWorkspace="ws_shape", DataX=[1, 1], DataY=[2, 2])
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertFalse(figure)

    # Sample, Container and Components
    def test_sample_container_and_components_valid(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(f'Sample, Container and Components: {workspace.name()}', figure.gca().get_title())

    def test_plot_title_for_other_component_arrangements_that_are_less_likely(self):
        self.assertEqual('Container and Components: workspace_name',
                         sample_shape.construct_title(False, True, True, "workspace_name"))
        self.assertEqual('Sample and Components: workspace_name',
                         sample_shape.construct_title(True, False, True, "workspace_name"))
        self.assertEqual('Components: workspace_name',
                         sample_shape.construct_title(False, False, True, "workspace_name"))

    @patch("mantidqt.plotting.sample_shape.add_beam_arrow")
    def test_add_beam_arrow_called_once(self, mock_add_beam_arrow):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(1, mock_add_beam_arrow.call_count)

    @patch("mantidqt.plotting.sample_shape.add_arrow")
    def test_add_arrow_called_once(self, mock_add_arrow):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual(1, mock_add_arrow.call_count)

    @patch("mantidqt.plotting.sample_shape.call_set_mesh_axes_equal")
    @patch("mantidqt.plotting.sample_shape.add_beam_arrow")
    def test_add_beam_arrow_called_after_call_set_mesh_axes_equal(self, mock_add_beam_arrow,
                                                                  mock_call_set_mesh_axes_equal):
        manager = Mock()
        manager.attach_mock(mock_call_set_mesh_axes_equal, "mock_call_set_mesh_axes_equal")
        manager.attach_mock(mock_add_beam_arrow, "mock_add_beam_arrow")

        workspace = setup_workspace_sample_container_and_components_from_mesh()
        figure = sample_shape.plot_sample_container_and_components(workspace.name())
        self.assertTrue(figure)
        self.assertEqual([each_call[0] for each_call in manager.mock_calls],
                         ['mock_call_set_mesh_axes_equal', 'mock_add_beam_arrow'])

    def test_beam_direction(self):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        source = workspace.getInstrument().getSource()
        sample = workspace.getInstrument().getSample()
        beam_direction = sample_shape.calculate_beam_direction(source, sample)
        assert_array_equal([0, 0, 12.75], beam_direction)

    def test_lattice_vectors(self):
        workspace = CreateSampleWorkspace()
        SetUB(workspace, a=1, b=1, c=2, alpha=90, beta=90, gamma=60)
        real_lattice_vectors, reciprocal_lattice_vectors = sample_shape.calculate_lattice_vectors(workspace)
        expected_real = [[3.14159265e+00,  6.28318531e+00,  0.00000000e+00],
                         [-1.40822468e-16,  3.84734139e-16,  1.25663706e+01],
                         [5.44139809e+00,  0.00000000e+00,  0.00000000e+00]]
        assert_allclose(expected_real, real_lattice_vectors, rtol=self.RELATIVE_TOLERANCE)
        expected_reciprocal = [[0.000000e+00,  1.000000e+00, -3.061617e-17],
                               [0.000000e+00,  0.000000e+00,  5.000000e-01],
                               [1.154701e+00, -5.773503e-01,  3.061617e-17]]
        assert_allclose(expected_reciprocal, reciprocal_lattice_vectors, rtol=self.RELATIVE_TOLERANCE)

    @patch("mantidqt.plotting.sample_shape.call_set_mesh_axes_equal")
    def test_limits_fed_to_call_set_mesh_axes_equal(self, mock_call_set_mesh_axes_equal):
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        _ = sample_shape.plot_sample_container_and_components(workspace.name())
        write_calls = mock_call_set_mesh_axes_equal.call_args_list
        assert_allclose(np.array([-0.0359, -0.0359, -0.01481, 0.0359, 0.0359, 0.01482]),
                        write_calls[0][0][-1], rtol=self.RELATIVE_TOLERANCE)

    @patch("mantidqt.plotting.sample_shape.overall_limits_for_every_axis")
    @patch("mantidqt.plotting.sample_shape.get_valid_component_shape_from_workspace")
    @patch("mantidqt.plotting.sample_shape.get_valid_container_shape_from_workspace")
    @patch("mantidqt.plotting.sample_shape.get_valid_sample_shape_from_workspace")
    def test_overall_limits_calls_get_valid_shape_methods(self, mock_get_valid_sample, mock_get_valid_container,
                                                          mock_get_valid_component, mock_overall_limits):
        mock_overall_limits.return_value = [1, 2, 3, 4, 5, 6]
        workspace = setup_workspace_sample_container_and_components_from_mesh()
        sample_shape.overall_limits_for_all_meshes(workspace)
        self.assertEqual(1, mock_get_valid_sample.call_count)
        self.assertEqual(1, mock_get_valid_container.call_count)
        self.assertEqual(8, mock_get_valid_component.call_count)


if __name__ == '__main__':
    main()
