# Building and running the chain of Docker images

This directory segregates the Dockerfiles to avoid clutter.

Assume you have already checked out the commit you want to build for.
Set an environment variable for convenience.

    REF=`git show -s --pretty=format:"%h"`

## Building

Note that the examples show me tagging the builds in the `gmxapi` dockerhub namespace.
If you don't have access to it, you can remove `gmxapi/` from the `-t` argument or use
a different dockerhub project space.

The different Dockerfiles require different build contexts.

For `gromacs-dependencies`, the build context doesn't matter.

    docker build -t gmxapi/gromacs-dependencies:mpich -f gromacs-dependencies.dockerfile .

For `gromacs`, the build context needs to be the root of the GROMACS repository.

    docker build -t gmxapi/gromacs:${REF} --build-arg DOCKER_CORES=4 -f gromacs.dockerfile ../..

For integration testing here, we only want the `python_packaging` subdirectory.

    docker build -t gmxapi/ci:${REF} --build-arg REF=${REF} -f ci.dockerfile ..

## Running

The `entrypoint.sh` script activates the python venv and wraps commands in a `bash` `exec`.
The default command is a script sourced from `../scripts/run_pytest.sh`. You can use this,
other scripts, `bash`, etc.

    docker run --rm -t gmxapi/ci:${REF}
    docker run --rm -t gmxapi/ci:${REF} /home/testing/scripts/run_pytest_mpi.sh
    docker run --rm -t gmxapi/ci:${REF} bash

To be able to step through with gdb, run with something like the following, replacing
'imagename' with the name of the docker image built with this recipe.

    docker run --rm -ti --security-opt seccomp=unconfined imagename bash
