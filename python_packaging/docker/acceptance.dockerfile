# Provide an easy-to-reproduce environment in which to run the exploratory
# notebook or acceptance tests.

# Build GROMACS with modifications for gmxapi
# See https://github.com/kassonlab/gromacs-gmxapi for more information.
# This image serves as a base for integration with the gmxapi Python tools and sample code.

FROM jupyter/scipy-notebook as intermediate

# Allow the build type to be specified with `docker build --build-arg TYPE=something`
ARG TYPE=Release
# Allow build for an arbitrary branch or tag, but default to the tip of `master`
ARG BRANCH=master
ARG REPO=https://github.com/gromacs/gromacs.git

# jupyter/scipy-notebook sets the USER to jovyan. Temporarily switch it back to root to install more stuff.
USER root

RUN \
    apt-get update && \
    apt-get -y upgrade && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        byobu \
        curl \
        libfftw3-dev \
        libopenblas-dev \
        libopenmpi-dev \
        git \
        software-properties-common \
        unzip && \
    rm -rf /var/lib/apt/lists/*
# By putting the above commands together, we avoid creating a large intermediate layer with all of /var/lib/apt/lists

USER jovyan

RUN cd /home/jovyan && \
    git clone --depth=1 --no-single-branch $REPO gromacs-source && \
    (cd /home/jovyan/gromacs-source && \
        git checkout $BRANCH)

# The CMake available with apt-get may be too old.
RUN bash -c "source /home/jovyan/gromacs-source/ci_scripts/set_compilers && \
    /home/jovyan/gromacs-source/ci_scripts/install_cmake.sh"

# Note: AVX2 instructions not available in older docker engines.
RUN mkdir -p /home/jovyan/gromacs-build && \
    cd /home/jovyan/gromacs-build && \
    rm -f CMakeCache.txt && \
    /home/jovyan/install/cmake/bin/cmake /home/jovyan/gromacs-source \
        -DCMAKE_INSTALL_PREFIX=/home/jovyan/install/gromacs \
        -DGMXAPI=ON \
        -DGMX_THREAD_MPI=ON \
        -DGMX_BUILD_HELP=OFF \
        -DGMX_SIMD=AVX_256 \
        -DGMX_USE_RDTSCP=OFF \
        -DGMX_HWLOC=OFF \
        -DCMAKE_BUILD_TYPE=$TYPE && \
    make -j4 install



FROM jupyter/scipy-notebook

# jupyter/scipy-notebook sets the USER to jovyan. Temporarily switch it back to root to install more stuff.
USER root

RUN \
    apt-get update && \
    apt-get -y upgrade && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        byobu \
        curl \
        doxygen \
        libfftw3-dev \
        libopenblas-dev \
        libopenmpi-dev \
        gdb \
        git \
        graphviz \
        htop \
        man \
        mscgen \
        openmpi-bin \
        ssh \
        software-properties-common \
        unzip \
        vim \
        wget && \
    rm -rf /var/lib/apt/lists/*

USER jovyan
RUN conda install pytest tox cmake

COPY --from=intermediate /home/jovyan/install /home/jovyan/install

ADD --chown=jovyan:users acceptance /home/jovyan/acceptance

# MPI tests can be run in this container without requiring MPI on the host.
# We should also try tests with an MPI-connected set of docker containers.

# To be able to step through with gdb, run with something like the following, replacing
# 'imagename' with the name of the docker image built with this recipe.
# docker run --rm -ti --security-opt seccomp=unconfined imagename bash
