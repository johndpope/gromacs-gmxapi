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
from gmxapi import util

# import gmxapi.version


# Module-level logger
logger = logging.getLogger(__name__)
logger.info('Importing gmxapi._commandline_operation')

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

def function_wrapper(output=()):
    """Generate a decorator for wrapped functions with signature manipulation.

    New function accepts the same arguments except for 'output', which is a data structure
    generated according to `output` in `function_wrapper`.

    The new function returns an object with an `output` attribute containing the named outputs.

    Example:
        @function_wrapper(output=['spam', 'foo'])
        def myfunc(parameter=None, output=None):
            output.spam = parameter
            output.foo = parameter

        operation1 = myfunc('spam spam')
        assert operation1.output.spam.result() == 'spam spam'
        assert operation1.output.foo.result() == 'spam spam'
    """
    # TODO: gmxapi operations need to allow a context-dependent way to generate an implementation with input.
    # This function wrapper reproduces the wrapped function's kwargs, but does not allow chaining a
    # dynamic `input` kwarg and does not dispatch according to a `context` kwarg. We should allow
    # a default implementation and registration of alternate implementations. We don't have to do that
    # with functools.singledispatch, but we could, if we add yet another layer to generate a wrapper
    # that takes the context as the first argument.

    output_names = list([name for name in output])
    def decorator(function):
        @functools.wraps(function)
        def new_helper(*args, **kwargs):

            class OutputResourceMeta(type):
                def __new__(mcs, name, bases, class_dict):
                    for attr in output_names:
                        # TODO: Replace None with an appropriate descriptor
                        class_dict[attr] = None
                    cls = type.__new__(mcs, name, bases, class_dict)
                    return cls

            class OutputResource(object, metaclass=OutputResourceMeta):
                pass

            class DataProxyMeta(type):
                def __new__(mcs, name, bases, class_dict):
                    for attr in output_names:
                        # TODO: Replace None with an appropriate (read-only) descriptor
                        class_dict[attr] = None
                    cls = type.__new__(mcs, name, bases, class_dict)
                    return cls

            class OutputDataProxy(object, metaclass=DataProxyMeta):
                # TODO: we want some container behavior, in addition to the attributes...
                pass

            class Resources(object):
                # TODO: should be an implementation detail of the Context, valid only during session lifetime.
                __slots__ = ['output']

            class Operation(object):
                @property
                def _output_names(self):
                    return [name for name in output_names]

                def __init__(self, *args, **kwargs):
                    ##
                    # TODO: generate type at class level, not instance at instance level
                    output_publishing_resource = OutputResource()
                    output_data_proxy = OutputDataProxy()
                    for accessor in self._output_names:
                        setattr(output_publishing_resource, accessor, None)
                        setattr(output_data_proxy, accessor, None)
                    ##
                    self._resources = Resources()
                    self._resources.output = output_publishing_resource
                    self._output = output_data_proxy
                    self.input_args = tuple(args)
                    self.input_kwargs = {key: value for key, value in kwargs.items()}

                @property
                def output(self):
                    return self._output

                def run(self):
                    # TODO: take action only if outputs are not satisfied.
                    # TODO: make sure this gets run if outputs need to be satisfied for `result()`
                    function(*self.input_args, output=self._resources.output, **self.input_kwargs)
                    # TODO: Add publishing infrastructure to connect output resources to published output.
                    for name in output_names:
                        setattr(self._output, name, getattr(self._resources.output, name))

            operation = Operation(*args, **kwargs)
            return operation

        return new_helper

    return decorator

# Replace cli() with a wrapping function that
#    * strips the `output` argument from the signature
#    * provides `output` to the inner function and
#    * returns the output object when called with the shorter signature.
#
# Note: could be done as a decorator instead at the cli() definition.
#
cli = function_wrapper(output=['erroroutput', 'returncode'])(cli)

# Replace cli()
# Map the cli() `shell` input to the result of a `False` constant.

# Map the cli() `command` input to the result of a string concatenation operation.

# Note: this looks a lot like the subgraph preparation,
# but with clear inputs and outputs versus stateful variables and named sub-operations
def find_executable(executable):
    command = util.which(executable)
    if command is None or command == "":
        raise exceptions.UsageError("Need an executable command.")
    return [util.to_string(command)]

def concatenate_lists(sublists=()):
    # TODO: Each sublist or sublist element could be a "future" handle.
    full_list = []
    for sublist in sublists:
        if sublist is not None:
            full_list += sublist
    return full_list

def filemap_to_flag_list(filemap=None):
    flag_list = []
    if filemap is not None:
        for flag, value in filemap.items():
            flag_list.extend((flag, value))

def make_constant(value):
    return type(value)(value)

# TODO: Use generating function or decorator that can validate kwargs.
def commandline_operation(executable=None, arguments=None, input_files=None, output_files=None, label=None, context=None):
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
    command = concatenate_lists((find_executable(executable),
                                 arguments,
                                 filemap_to_flag_list(input_files),
                                 filemap_to_flag_list(output_files)))
    shell = make_constant(False)
    return cli(command=command, shell=shell)


#_commandline_operation = make_operation(cli, input=['command', 'shell', 'file'], output=['file', 'returncode', 'erroroutput'])
#commandline_operation = make_operation_helper(_commandline_operation, properties=['executable', 'arguments', 'input', 'output', 'shell'])

#
# def commandline_operation(executable=None, arguments=None, input=None, output=None, shell=False):
#     """Execute a command line program in a subprocess.
#
#     Configure an executable in a subprocess. Executes when run in an execution
#     Context, as part of a work graph or via gmx.run(). Runs in the current
#     working directory.
#
#     Shell processing is not enabled, but can be considered for a future version.
#     This means that shell expansions such as environment variables, globbing (`*`),
#     and other special symbols (like `~` for home directory) are not available.
#     This allows a simpler and more robust implementation, as well as a better
#     ability to uniquely identify the effects of a command line operation. If you
#     think this disallows important use cases, please let us know.
#
#     Arguments:
#          arguments : a single tuple (or list) to be the first arguments to `executable`
#          input : mapping of command line flags to input filename arguments
#          output : mapping of command line flags to output filename arguments
#
#     Arguments are iteratively added to the command line with standard Python
#     iteration, so you should use a tuple or list even if you have only one parameter.
#     I.e. If you provide a string with `arguments="asdf"` then it will be passed as
#     `... "a" "s" "d" "f"`. To pass a single string argument, `arguments=("asdf")`
#     or `arguments=["asdf"]`.
#
#     `input` and `output` should be a dictionary with string keys, where the keys
#     name command line "flags" or options.
#
#     Example:
#         Execute a command named `exe` that takes a flagged option for file name
#         (stored in a local Python variable `my_filename`) and an `origin` flag
#         that uses the next three arguments to define a vector.
#
#             >>> import gmxapi
#             >>> from gmxapi import commandline_operation as cli
#             >>> my_filename = "somefilename"
#             >>> my_op = cli('exe', arguments=['--origin', 1.0, 2.0, 3.0], input={'-f': my_filename})
#             >>> gmxapi.run(my_op)
#
#     Returns:
#         An Operation to invoke local subprocess(es)
#
#     Note:
#         STDOUT is available if a consuming operation is bound to `output.stdout`.
#         STDERR is available if a consuming operation is bound to `output.stderr`.
#         Otherwise, STDOUT and/or STDERR is(are) closed when command is called.
#
#     Warning:
#         Commands using STDIN cannot be used and is closed when command is called.
#
#     Todo:
#         We can be more helpful about managing the work graph for operations, but
#         we need to resolve issue #90 and others first.
#
#     """
#     if shell != False:
#         raise exceptions.UsageError("Operation does not support shell processing.")
#
#     command = util.which(executable)
#     if command is None or command == "":
#         raise exceptions.UsageError("Need an executable command.")
#
#     command_args = [util.to_utf8(command)]
#     # If we can confirm that we are handling iterables well, we can update
#     # the documentation, revising the warning about single string arguments
#     # and noting the automatic conversion of single scalars.
#     #
#     # Strings are iterable, but we want to treat them as scalars.
#     if isinstance(arguments, (str, bytes)):
#         arguments = [arguments]
#     # Python 2 compatibility:
#     try:
#         if isinstance(arguments, unicode):
#             arguments = [arguments]
#     except NameError as e:
#         # Python 3 does not have unicode type
#         pass
#     if arguments is not None:
#         for arg in arguments:
#             command_args.append(util.to_utf8(arg))
#
#     if input is not None:
#         for kwarg, value in input.items():
#             # the flag
#             command_args.append(util.to_utf8(kwarg))
#             # the filename argument
#             command_args.append(util.to_utf8(value))
#
#     if output is not None:
#         for kwarg, value in input.items():
#             # the flag
#             command_args.append(util.to_utf8(kwarg))
#             # the filename argument
#             command_args.append(util.to_utf8(value))
#
#     if len(command_args) == 0:
#         raise exceptions.UsageError("No command line could be constructed.")
#     operation = _CommandLineOperation({'command': command_args, 'shell': shell})
#     return operation
