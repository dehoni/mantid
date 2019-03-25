# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
"""
api
===

Defines Python objects that wrap the C++ API namespace.

"""
from __future__ import absolute_import

###############################################################################
# Load the C++ library
###############################################################################
from mantid.utils import _shared_cextension, import_mantid

# insert all the classes from _api in the mantid.api namespace
with _shared_cextension():
    _api = import_mantid('._api', 'mantid.api', globals())

###############################################################################
# Add importAll member to ADS.
#
# Must be imported AFTER all the api members
# have been added to the mantid.api namespace above!
###############################################################################
from mantid.api import _adsimports

###############################################################################
# Make aliases accessible in this namespace
###############################################################################
from mantid.api._aliases import *

###############################################################################
# Attach additional operators to workspaces
###############################################################################
from mantid.api import _workspaceops

_workspaceops.attach_binary_operators_to_workspace()
_workspaceops.attach_unary_operators_to_workspace()
_workspaceops.attach_tableworkspaceiterator()
