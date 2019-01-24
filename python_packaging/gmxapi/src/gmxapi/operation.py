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

from gmxapi.exceptions import ApiError

class Runner(object):
    """Descriptor class for runner behavior."""
    def __init__(self, implementation):
        self.name = None
        self.internal_name = None

    def __get__(self, instance, instance_type):
        if instance is None: return self
        return getattr(instance, self.internal_name, lambda : True)

    def __set__(self, instance, value):
        setattr(instance, self.internal_name, value)

class Result(object):
    """Descriptor class for Result behavior."""
    pass

class OutputMeta(type):
    """Output proxy metatype to annotate and enforce interface."""
    def __new__(meta, name, bases, class_dict):
        for key, value in class_dict.items():
            if isinstance(value, Result):
                value.name = key
                value.internal_name = '_' + key
        cls = type.__new__(meta, name, bases, class_dict)
        return cls

class Output(object):
    """Descriptor class for output proxy behavior."""
    def __init__(self):
        self.name = None
        self.internal_name = None

    def __get__(self, instance, instance_type):
        if instance is None: return self
        return getattr(instance, self.internal_name, '')

    def __set__(self, instance, value):
        setattr(instance, self.internal_name, value)

class ValidateOperation(type):
    """Confirm that an Operation class implements the gmxapi Operation protocol."""
    def __new__(meta, name, bases, class_dict):
        # Don't validate the abstract Operation class
        if bases != (object,):
            if class_dict['run'] is None:
                raise ApiError()
            else:
                class_dict['run'].name = 'run'
                class_dict['run'].internal_name = '_' + 'run'
            if class_dict['output'] is None:
                raise ApiError()
        return type.__new__(meta, name, bases, class_dict)

class Operation(object, metaclass=ValidateOperation):
    run = None
    output = None

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
    operation_output = output
    if operation_output is None:
        operation_output = object()

    class NewOperation(Operation):
        run = Runner(implementation)
        output = operation_output

    def factory(*args, **kwargs):
        operation = NewOperation()
        return operation

    return factory
