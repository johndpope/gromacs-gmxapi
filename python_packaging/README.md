# Python package sources

This directory provides the files that will be copied to the GROMACS installation location from which users may 
install Python packages.
This allows C++ extension modules to be built against a user-chosen GROMACS installation,
but for a Python interpreter that is very likely different from that used
by the system administrator who installed GROMACS.

## gmxapi

Python framework for gmxapi high-level interface. Contains functionality not ready or not destined for gromacs 
bindings package.

User:

    source $HOME/somevirtualenv/bin/activate
    pip install -r /path/to/gromacs/python/requirements.txt
    pip install /path/to/gromacs/python/gmxapi
    python -c 'import gmxapi as gmx'

## gromacs

gmxapi Python implementation (bindings against GROMACS installed interface).
May be used directly (for canonical releases) or indirectly (as a dependency of an experimental front-end).

User:

    source $HOME/somevirtualenv/bin/activate
    pip install --upgrade scikit-build
    pip install /path/to/gromacs/python/gromacs
    python -c 'import gromacs as gmx'

## pybind11

Python bindings are expressed in C++ using the pybind11 template library.
The pybind11 repository is mirrored in GROMACS project sources and
installed with GROMACS for convenience and reproducibility.

pybind11 sources were added to the repository with `git-subtree` and a squash merge.

To update the version of pybind11 included with the GROMACS repository,
`git-subtree` uses meta-data stashed in the git history to prepare an appropriate squash merge.

# Cross compiling

On some systems, GROMACS will have been built and installed for a different
architecture than the system on which the Python package will be compiled.
We need to use CMake Tool-chains to support cross-compiling for the target architecture.

Note: scikit-build can use CMake Toolchains to properly handle `pip` builds.

# Offline installation

The `pip install` options `--no-index` and `--find-links` allow for an offline stash of package archives so that 
satisfying dependencies for a new virtualenv does not require network access or lengthy build times.
