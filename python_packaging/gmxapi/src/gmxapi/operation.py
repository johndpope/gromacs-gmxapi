"""Define gmxapi-compliant Operations

    # make_operation is a utility used to create the `noop` module attribute
    # in the importable `test_support` module that copies its `data` input to its output.
    from test_support import noop
    op = noop(input={'data': True})
    assert op.output.data.extract() == True
    op = noop(input={'data': False})
    assert op.output.data.extract() == False

    # the `count` operation copies its `data` input to its output
    # and increments its `count` input to its output.
    from test_support import count
    op1 = count(input={'data': True})
    op2 = count(input=op1.output)
    op2.run()
    # TBD: how to introspect execution dependency or not-yet-executed status?
    # To allow introspection in testing, we might use module global data for operations executed in the local process.

    op1 = count(input={'data': False, 'count': 0})
    op2 = count(input=op1.output)
    assert op2.output.count.extract() == 2
"""

class Operation(object):
    def output(self):
        pass
    def run(self):
        pass

def make_operation(implementation, input=None, output=None):
    """Utility to create a gmxapi-compliant Operation in an importable module.

    Args:
        implementation : importable class that will be instantiated to execute the operation.

    Keyword Arguments:
        input : list of parameters to accept as keyword arguments and relay to the constructor.
        output : list of outputs to map from instance properties or methods.

    Returns:
        A callable that produces objects supporting the gmxapi operation protocol.

    """
    def factory(*args, **kwargs):
        operation = Operation()
        return operation

    return factory
