#!/usr/bin/env python
import os
import unittest

from gmxapi import util


class WhichUtilTestCase(unittest.TestCase):
    """test util.which"""
    def test_find_executable(self):
        # This command exists pretty much everywhere...
        executable = '/usr/bin/env'
        if os.path.exists(executable):
            assert util.which('env') == '/usr/bin/env'


if __name__ == '__main__':
    unittest.main()
