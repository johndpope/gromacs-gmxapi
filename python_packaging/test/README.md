# Tests for Python packages distributed with GROMACS

## Requirements

Python tests use the `unittest` standard library module and the `unittest.mock`
submodule, included in Python 3.3+.

The additional `pytest` package allows tests to be written more easily and
concisely, with easier test fixtures (through decorators), log handling, and
other output handling.

## Files

Python files beginning with `test_` are collected by the Python testing
framework during automatic test discovery.

`conftest.py` and `pytest.ini` provide configuration for `pytest`.

`sampleoperation.py` is a module that test scripts can load to mimic external
Python code in user scripts.

## Usage

For basic tests, install the Python package(s) (presumably into a virtualenv),
then use the `pytest` executable to run these tests against the installed
package(s).

`pytest $LOCAL_REPO_DIR/python/test`

where `$LOCAL_REPO_DIR` is the path to the local copy of the GROMACS source repository.

For multi-process tests, run with an MPI execution wrapper and the `mpi4py` module.

`mpiexec -n 2 python -m mpi4py -m pytest $LOCAL_REPO_DIR/python/test`
