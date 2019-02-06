"""Utility functions supporting the Gromacs Python interface.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

__all__ = ['to_utf8', 'to_string', 'which']

import sys
import os

from gmxapi import exceptions
# from gmx._commandline_operation import commandline_operation

def to_utf8(input):
    """Return a utf8 encoded byte sequence of the Unicode ``input`` or its string representation.

    In Python 2, returns a :py:str object. In Python 3, returns a :py:bytes object.
    """
    py_version = sys.version_info.major
    if py_version == 3:
        if isinstance(input, str):
            value = input.encode('utf-8')
        elif isinstance(input, bytes):
            value = input
        else:
            try:
                string = str(input)
            except:
                raise exceptions.UsageError("Cannot find a Python 3 string representation of input.")
            value = string.encode('utf-8')
    else:
        assert py_version == 2
        if isinstance(input, unicode):
            value = input.encode('utf-8')
        else:
            try:
                value = str(input)
            except:
                raise exceptions.UsageError("Cannot find a Python 2 string representation of input.")
    return value

def to_string(input):
    """Return a Unicode string representation of ``input``.

    If ``input`` or its string representation is not already a Unicode object, attempt to decode as utf-8.

    In Python 3, returns a native string, decoding utf-8 encoded byte sequences if necessary.

    In Python 2, returns a Unicode object, converting data types if necessary and possible.

    Note:
        In Python 2, byte sequence objects are :py:str type, so passing a :py:str actually converts from :py:str: to
        :py:unicode. To guarantee a native string object, wrap the output of this function in ``str()``.
    """
    py_version = sys.version_info.major
    if py_version == 3:
        if isinstance(input, str):
            value = input
        else:
            try:
                value = input.decode('utf-8')
            except:
                try:
                    value = str(input)
                except:
                    raise exceptions.UsageError("Cannot find a Python 3 string representation of input.")
    else:
        assert py_version == 2
        if isinstance(input, unicode):
            value = input
        else:
            try:
                string = str(input)
            except:
                raise exceptions.UsageError("Cannot find a Python 2 string representation of input.")
            value = string.decode('utf-8')
    return value

def which(command):
    """
    Get the full path of an executable that can be resolved by the shell.

    :param command: executable in the user's PATH
    :return: Absolute path of executable.

    Ref: https://stackoverflow.com/questions/377017/test-if-executable-exists-in-python
    """
    try:
        # TODO: we might want an abstraction of character encoding for filesystem and argv / shell strings
        command_path = to_utf8(command)
    except:
        raise exceptions.ValueError("Argument must be representable on the command line.")
    if os.path.exists(command_path):
        command_path = os.path.abspath(command_path)
        if os.access(command_path, os.X_OK):
            return command_path
    else:
        # Try to find the executable on the default PATH
        try:
            # available in Python 3
            from shutil import which
        except:
            # Python 2 compatibility, from
            # https://stackoverflow.com/a/377028/5351807
            def which(program):
                import os
                is_exe = lambda fpath: os.path.isfile(fpath) and os.access(fpath, os.X_OK)

                fpath, fname = os.path.split(program)
                if fpath:
                    if is_exe(program):
                        return program
                else:
                    for path in os.environ["PATH"].split(os.pathsep):
                        exe_file = os.path.join(path, program)
                        if is_exe(exe_file):
                            return exe_file
                return None

        return which(command)

def _test():
    import doctest
    doctest.testmod()

if __name__ == "__main__":
    _test()
