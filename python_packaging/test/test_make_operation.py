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

"""Test tools for producing gmxapi-compliant Operations.

* gmx.make_operation wraps importable Python code.
* gmx.make_operation produces output proxy that establishes execution dependency
* gmx.make_operation produces output proxy that can be used as input
* gmx.make_operation uses dimensionality and typing of named data to generate correct work topologies
"""

import unittest
import pytest

from pytesthelpers import withmpi_only

import sampleoperation

# Find package or skip the tests while installable gmx package does not yet exist.
# TODO: update as implemented.
try:
    # First look for experimental front-end package.
    from gmxapi.operation import make_operation
    gmx_package_name = 'gmxapi'
except ImportError:
    # Next try core bindings package.
    try:
        from gromacs.operation import make_operation
        gmx_package_name = 'gromacs'
    except ImportError:
        # Fall back to older external package, if available.
        gmx_package_name = 'gmx'

# Effectively, `import gmx_package_name as gmx` or skip test
gmx = pytest.importorskip(gmx_package_name)

# Decorator to mark tests that are expected to fail
xfail = pytest.mark.xfail

# We do not import sampleoperation here because we want to test its importability separately
#import sampleoperation

@pytest.mark.usefixtures('cleandir')
class TestOperationWrapper(unittest.TestCase):
    """Test that the gmx.make_operation helper generates valid Operations."""
    @xfail
    def test_trivial_operation(self):
        """Check that the make_operation utility produces a callable correctly."""
        # Create Operation object
        import sampleoperation
        my_op = sampleoperation.noop(input=None)
        # Check for expected Operation interfaces
        assert hasattr(my_op, 'output')
        assert hasattr(my_op, 'run')
        # Check for expected behavior in various contexts
        assert my_op.output is not None
        result = my_op.run()

        assert result is not None

        # sampleoperation.noop always has work to perform: result is always still runnable.
        # Protocols regarding rerunability of operations or the Session execution are
        # still a bit up in the air, so this is commented out for now.
        # assert next(result, None) is not None

        # handle explicitly immediate (non-default) context
        my_op = sampleoperation.noop(input=None, context=gmx.context.immediate)
        my_op.run()

        # handle graph aware context
        my_context = gmx.context.graph
        my_op = sampleoperation.noop(input=None, context=my_context)

        # execute from graph context...
        # TODO
        assert False


    @withmpi_only
    @xfail
    def test_mpi_trivial(self):
        """Correctly execute (trivial) work in a multiprocessing environment.

        Execute exactly the requested work, regardless of parallelism.

        Test:
        * deterministically produce unique output in a temporary file (system name, timestamp, MPI rank)
        * move it into place (filesystem move operations are atomic)
        * confirm that exactly one operation was executed (use logical operation via mpi4py)
        """
        assert True

    @xfail
    def test_single_operation(self):
        """Check that the make_operation utility produces a callable correctly.

        Use an operation that increments a global counter when called to show
        that behavior is as expected.
        """
        # Create Operation object: a counter initialized to the value '5'
        import sampleoperation
        initial_count = 5
        my_op = sampleoperation.count(count=initial_count)
        assert my_op.count == 5

        # Counter has no completion criteria, and could be run again.
        result = my_op.run()
        assert result is not None

        assert result.count == initial_count + 1

        #

        # handle explicitly immediate (non-default) context
        my_op = sampleoperation.noop(input=None, context=gmx.context.immediate)
        my_op.run()

        # handle graph aware context
        my_context = gmx.graph
        my_op = sampleoperation.noop(input=None, context=my_context)

        # execute from graph context...
        assert True

    @xfail
    def test_dependency(self):
        """Check that a chain of operations executes sequentially."""
        assert True

    @xfail
    def test_data_flow(self):
        """Check that output of one operation can be used as input to another."""
        assert True

    @xfail
    def test_topology(self):
        """Check for correct topology of work graph.

        Dimensionality and typing of named data to generates correct work
        topologies.
        """
        assert True

    @withmpi_only
    @xfail
    def test_mpi_topology(self):
        assert True


if __name__ == '__main__':
    unittest.main()
