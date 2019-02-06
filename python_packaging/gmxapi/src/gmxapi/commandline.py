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

#__all__ = ['cli_function', 'cli_class']
#__all__ = ['cli_class']
__all__ = ['cli_function']

from gmxapi._cli.cli_function import commandline_operation as cli_function
#from gmxapi._cli.cli_class import operation_helper as cli_class
