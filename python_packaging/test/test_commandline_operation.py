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

"""Test the gmxapi.commandline_operation wrapper tool.

gmxapi.commandline_operation() provides additional logic over gmxapi.make_operation
to conveniently wrap command line tools.
"""

import os
import pytest
import stat
import tempfile
import unittest

from gmxapi.commandline import cli_function as commandline_operation
from gmxapi import util

# Decorator to mark tests that are expected to fail
xfail = pytest.mark.xfail

class CommandLineOperationSimpleTestCase(unittest.TestCase):
    """Test creation and execution of command line wrapper."""
    def test_true(self):
        operation = commandline_operation(executable='true')
        # Note: 'stdout' and 'stderr' not mapped.
        # Note: getitem not implemented.
        # assert not 'stdout' in operation.output
        # assert not 'stderr' in operation.output
        assert not hasattr(operation.output, 'stdout')
        assert not hasattr(operation.output, 'stderr')
        assert hasattr(operation.output, 'file')
        assert hasattr(operation.output, 'erroroutput')
        assert hasattr(operation.output, 'returncode')
        operation.run()
        # assert operation.output.returncode.result() == 0
        assert operation.output.returncode == 0

    def test_false(self):
        operation = commandline_operation(executable='false')
        operation.run()
        assert operation.output.returncode == 1

    def test_echo(self):
        # TODO: do we want to pipeline or checkpoint stdout somehow?
        operation = commandline_operation(executable='echo', arguments=['hi there'])
        operation.run()
        assert operation.output.returncode == 0

class CommandLineOperationPipelineTestCase(unittest.TestCase):
    """Test dependent sequence of operations."""
    def test_operation_dependence(self):
        """Confirm that dependent operations are only executed after their dependencies.

        In a sequence of two operations, write a two-line file one line at a time.
        Use a user-provided filename as a parameter to each operation.
        """
        with tempfile.TemporaryDirectory() as directory:
            fh, filename = tempfile.mkstemp(dir=directory)
            os.close(fh)

            line1 = 'first line'
            subcommand = ' '.join(['echo', '"{}"'.format(line1), '>>', filename])
            commandline = ['-c', subcommand]
            filewriter1 = commandline_operation('bash', arguments=commandline)

            line2 = 'second line'
            subcommand = ' '.join(['echo', '"{}"'.format(line2), '>>', filename])
            commandline = ['-c', subcommand]
            filewriter2 = commandline_operation('bash', arguments=commandline, input=filewriter1)

            filewriter2.run()
            # Check that the file has the two expected lines
            with open(filename, 'r') as fh:
                lines = [text.rstrip() for text in fh]
            assert len(lines) == 2
            assert lines[0] == line1
            assert lines[1] == line2

    @xfail
    def test_data_dependence(self):
        """Confirm that data dependencies correctly establish resolvable execution dependencies.

        In a sequence of two operations, write a two-line file one line at a time.
        Use the output of one operation as the input of another.
        """
        with tempfile.TemporaryDirectory() as directory:
            file1 = os.path.join(directory, 'input')
            file2 = os.path.join(directory, 'output')

            # Make a shell script that acts like the type of tool we are wrapping.
            scriptname = os.path.join(directory, 'clicommand.sh')
            with open(scriptname, 'w') as fh:
                fh.writelines(['#!' + util.which('bash'),
                               '# Concatenate an input file and a string argument to an output file.',
                               '# Mock a utility with the tested syntax.',
                               '#     clicommand.sh "some words" -i inputfile -o outputfile',
                               'echo $1 | cat - $3 > $5'])
            os.chmod(scriptname, stat.S_IRWXU)

            line1 = 'first line'
            filewriter1 = commandline_operation(scriptname,
                                                input_files={'-i': os.devnull},
                                                output_files={'-o': file1})

            line2 = 'second line'
            filewriter2 = commandline_operation(scriptname,
                                                input_files={'-i': filewriter1.output.file['-o']},
                                                output_files={'-o': file2})

            filewriter2.run()
            # Check that the files have the expected lines
            with open(file1, 'r') as fh:
                lines = [text.rstrip() for text in fh]
            assert len(lines) == 1
            assert lines[0] == line1
            with open(file2, 'r') as fh:
                lines = [text.rstrip() for text in fh]
            assert len(lines) == 2
            assert lines[0] == line1
            assert lines[1] == line2

    @xfail
    def test_parallel_data_dependence(self):
        """As in test_data_dependence, but with two independent data flows."""
        assert True
    @xfail
    def test_broadcast_data_dependence(self):
        """As in test_data_dependence, but with one operation feeding two independent consumers."""
        assert True