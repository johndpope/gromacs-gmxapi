"""
Provide command line operation for the basic Context.

:func:`commandline` Helper function adds an operation to the work graph
and returns a proxy to the Python interpreter. Imported by :module:`gmx`.

:func:`_commandline` utility function provides callable function object to execute
a command line tool in a subprocess. Imported by :module:`gmx.util` for standalone
operation and by the session runner for work graph execution.

:module:`gmx.context` maps the `gmxapi.commandline` operation to
:func:`_gmxapi_graph_director` in this module to get a Director that can be used
during Session launch.

TODO: I am testing a possible convention for helper function, factory function, and implementation.
For the ``gmxapi`` work graph operation name space, `gmx.context` looks for
operations in ``gmx._<name>``, where *<name>* is the operation name. We will
probably adopt a slightly different convention for the ``gromacs`` name space.
For all other name spaces, the package attempts to import the namespace as a
module and to look for operation implementations as a module attribute matching
the name of the operation. The attribute may be a callable or an object implementing
the Operation interface. Such an object may itself be a module.
(Note this would also provide a gateway to mixing Python and C++ in plugins,
since the namespace module may contain other code or import additional modules.)

The work graph node ``output`` contains a ``file`` port that is
a gmxapiMap output of command line arguments to filenames.

.. versionadded:: 0.0.8

    Graph node structure::
    'elements': {
        'cli_op_aaaaaa': {
            'label': 'exe1',
            'namespace': 'gmxapi',
            'operation': 'commandline',
            'params': {
                'executable': [],
                'arguments': [[]],
            },
            'depends': []
        },
        'cli_op_bbbbbb': {
            'label': 'exe2',
            'namespace': 'gmxapi',
            'operation': 'commandline',
            'params': {
                'executable': [],
                'arguments': [[], []],
            },
            'depends': ['cli_op_aaaaaa']
        },
    }

.. versionchanged:: 0.1

    Output ports are determined by querying the operation.

    Graph node structure::
    'elements': {
        'filemap_aaaaaa': {
            operation: 'make_map',
            'input': {
                '-f': ['some_filename'],
                '-t': ['filename1', 'filename2']
            },
            # 'output': {
            #     'file': 'gmxapi.Map'
            # }
        },
        'cli_op_aaaaaa': {
            'label': 'exe1',
            'namespace': 'gmxapi',
            'operation': 'commandline',
            'input': {
                'executable': [],
                'arguments': [[]],
                # this indirection avoids naming complications. Alternatively,
                # we could make parsing recursive and allow arbitrary nesting
                # with special semantics for dictionaries (as well as lists)
                'input_file_arguments': 'filemap_aaaaaa',
            },
            # 'output': {
            #     'file': 'gmxapi.Map'
            # }
        },
        'filemap_bbbbbb: {
            'label': 'exe1_output_files',
            'namespace': 'gmxapi',
            'operation': 'make_map',
            'input': {
                '-in1': 'cli_op_aaaaaa.output.file.-o',
                '-in2': ['static_fileB'],
                '-in3': ['arrayfile1', 'arrayfile2'] # matches dimensionality of inputs
            }
        },
        'cli_op_bbbbbb': {
            'label': 'exe2',
            'namespace': 'gmxapi',
            'operation': 'commandline',
            'input': {
                'executable': [],
                'arguments': [],
                'input_file_arguments': 'filemap_bbbbbb'
            },
        },

    }

"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

__all__ = []

import functools
import importlib
import os
from os import devnull
import subprocess
import tempfile
import warnings

from gmxapi import exceptions
from gmxapi import logging
from gmxapi.operation import computed_result, function_wrapper, concatenate_lists, make_constant
from gmxapi import util

# import gmxapi.version


# Module-level logger
logger = logging.getLogger(__name__)
logger.info('Importing gmxapi._commandline_operation')

#
# Replace cli() with a wrapping function that
#    * strips the `output` argument from the signature
#    * provides `output` to the inner function and
#    * returns the output object when called with the shorter signature.
#
@function_wrapper(output={'file': dict, 'erroroutput': str, 'returncode': int})
def cli(command=None, shell=None, output=None):
    """Execute a command line program in a subprocess.

    Configure an executable in a subprocess. Executes when run in an execution
    Context, as part of a work graph or via gmx.run(). Runs in the current
    working directory.

    Shell processing is not enabled, but can be considered for a future version.
    This means that shell expansions such as environment variables, globbing (`*`),
    and other special symbols (like `~` for home directory) are not available.
    This allows a simpler and more robust implementation, as well as a better
    ability to uniquely identify the effects of a command line operation. If you
    think this disallows important use cases, please let us know.

    Arguments:
         arguments : a single tuple (or list) to be the first arguments to `executable`
         input : mapping of command line flags to input filename arguments
         output : mapping of command line flags to output filename arguments

    Arguments are iteratively added to the command line with standard Python
    iteration, so you should use a tuple or list even if you have only one parameter.
    I.e. If you provide a string with `arguments="asdf"` then it will be passed as
    `... "a" "s" "d" "f"`. To pass a single string argument, `arguments=("asdf")`
    or `arguments=["asdf"]`.

    `input` and `output` should be a dictionary with string keys, where the keys
    name command line "flags" or options.

    Example:
        Execute a command named `exe` that takes a flagged option for file name
        (stored in a local Python variable `my_filename`) and an `origin` flag
        that uses the next three arguments to define a vector.

            >>> my_filename = "somefilename"
            >>> result = cli(('exe', '--origin', 1.0, 2.0, 3.0, '-f', my_filename), shell=False)
            >>> assert hasattr(result, 'file')
            >>> assert hasattr(result, 'erroroutput')
            >>> assert hasattr(result, 'returncode')

    Returns:
        A data structure with attributes for each of the results `file`, `erroroutput`, and `returncode`

    Result object attributes:
        * `file`: the mapping of CLI flags to filename strings resulting from the `output` kwarg
        * `erroroutput`: A string of error output (if any) if the process failed.
        * `returncode`: return code of the subprocess.

    """
    # Note: we could make provisions for stdio filehandles in a future version. E.g.
    # * STDOUT is available if a consuming operation is bound to `output.stdout`.
    # * STDERR is available if a consuming operation is bound to `output.stderr`.
    # * Otherwise, STDOUT and/or STDERR is(are) closed when command is called.
    #
    # Warning:
    #     Commands relying on STDIN cannot be used and is closed when command is called.

    # In the operation implementation, we expect the `shell` parameter to be intercepted by the
    # wrapper and set to False.
    if shell != False:
        raise exceptions.UsageError("Operation does not support shell processing.")

    try:
        command[0] = util.which(command[0])
    except:
        raise exceptions.ValueError('command argument could not be resolved to an executable file path.')

    # TODO: can these be a responsibility of the code providing 'resources'?
    stdin = open(devnull, 'r')
    stdout = open(devnull, 'w')
    stderr = open(devnull, 'w')

    erroroutput = None
    logger.debug('executing subprocess')
    with stdin as null_in, stdout as null_out, stderr as null_err:
        try:
            returncode = subprocess.check_call(command,
                                               shell=shell,
                                               stdin=null_in,
                                               stdout=null_out,
                                               stderr=null_err)
        except subprocess.CalledProcessError as e:
            logger.info("commandline operation had non-zero return status when calling {}".format(e.cmd))
            erroroutput = e.output
            returncode = e.returncode
    # resources.output.erroroutput.publish(erroroutput)
    # resources.output.returncode.publish(returncode)
    # `publish` is descriptive, but redundant. Access to the output data handler is
    # assumed to coincide with publishing, and we assume data is published when the
    # handler is released. A class with a single `publish` method is overly complex
    # since we can just use the assignment operator.
    output.erroroutput = erroroutput
    output.returncode = returncode

# This doesn't even need to be a separate operation. It can just generate the necessary data flow.
def filemap_to_flag_list(filemap=None):
    flag_list = []
    if filemap is not None:
        for flag, value in filemap.items():
            flag_list.extend((flag, value))
    # TODO: Should be a NDArray(dtype=str)
    return flag_list

# TODO: Use generating function or decorator that can validate kwargs.
# TODO: Outputs need to be fully formed and typed in the object returned from the helper (decorated function).
# Question: Should the implementing function be able to modify the work graph?
# Or should we have a restriction that implementation functions cannot call operations, and
# have more separation between the definition of the helper function and the definition of the implementation,
# allowing only helper functions to add to the work graph?
# At the moment, we don't have a way to prevent acquisition of new operation handles in runtime implementation functions,
# but we can discourage it for the moment and in the future we can check the current Context whenever getting a new operation
# handle to decide if it is allowed. Such a check could only be performed after the work is launched, of course.
def commandline_operation(executable=None, arguments=None, input_files=None, output_files=None, **kwargs):
    """Helper function to execute a subprocess in gmxapi data flow.

    Generate a chain of operations to process the named key word arguments and handle
    input/output data dependencies.

    Arguments:
        executable : name of an executable on the path
        arguments : list of positional arguments to insert at argv[1]
        input_files : mapping of command-line flags to input file names
        output_files : mapping of command-line flags to output file names
        label : optional user-friendly name for the operation handle
        context : optional execution context to declare owner of the operation results

    Output:
        The output node of the resulting operation handle contains
        * `file`: the mapping of CLI flags to filename strings resulting from the `output` kwarg
        * `erroroutput`: A string of error output (if any) if the process failed.
        * `returncode`: return code of the subprocess.

    """

    # TODO: Let the framework find the `result` for decorated operations
    # instead of calling shell.result()
    command = concatenate_lists((executable,
                                 arguments,
                                 filemap_to_flag_list(input_files),
                                 filemap_to_flag_list(output_files)))
    shell = make_constant(False)
    cli_args = {'command': command,
                'shell': shell}
    cli_args.update(kwargs)
    return cli(**cli_args)
