#!/usr/bin/env python
#
# This file is part of the GROMACS molecular simulation package.
#
# Copyright (c) 2019, by the GROMACS development team, led by
# Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
# and including many others, as listed in the AUTHORS file in the
# top-level source directory and at http://www.gromacs.org.
#
# GROMACS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1
# of the License, or (at your option) any later version.
#
# GROMACS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GROMACS; if not, see
# http://www.gnu.org/licenses, or write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
#
# If you want to redistribute modifications to GROMACS, please
# consider that scientific software is very special. Version
# control is crucial - bugs must be traceable. We will be happy to
# consider code for inclusion in the official distribution, but
# derived work must not be called official GROMACS. Details are found
# in the README & COPYING files - if they are missing, get the
# official version at http://www.gromacs.org.
#
# To help us fund GROMACS development, we humbly ask that you cite
# the research papers on the package. Check out http://www.gromacs.org.

"""Test gromacs.mdrun operation.

Factory produces deferred execution operation unless launch context /
session builder is provided.

Factory expects input for conformation, topology, simulation parameters, simulation state.

Factory accepts additional keyword input to indicate binding to the "potential"
interface.
"""

import unittest
import pytest

from pytesthelpers import withmpi_only

# Find package or skip the tests while installable gmx package does not yet exist.
# TODO: update as implemented.
try:
    # First look for experimental front-end package.
    from gmxapi import mdrun
    gmx_package_name = 'gmxapi'
except ImportError:
    # Next try core bindings package.
    try:
        from gromacs import mdrun
        gmx_package_name = 'gromacs'
    except ImportError:
        # Fall back to older external package, if available.
        gmx_package_name = 'gmx'

# Effectively, `import gmx_package_name as gmx` or skip test
gmx = pytest.importorskip(gmx_package_name)

# Decorator to mark tests that are expected to fail
xfail = pytest.mark.xfail

@pytest.mark.usefixtures('cleandir')
class TestLegacySimulationRunner(unittest.TestCase):
    pass

@pytest.mark.usefixtures('cleandir')
class TestMdrunOperation(unittest.TestCase):
    def setUp(self):
        import pkg_resources
        # Note: importing pkg_resources means setuptools is required for running this test.

        # Get or build TPR file from data bundled via setup(package_data=...)
        # Ref https://setuptools.readthedocs.io/en/latest/setuptools.html#including-data-files
        #from gmx.data import tprfilename
        self.tprfilename = tprfilename
    def test_run_from_tpr(self):
        md = gmx.read_tpr(self.tprfilename)

