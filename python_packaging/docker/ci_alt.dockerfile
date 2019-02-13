# Provide an easy-to-reproduce environment in which to test full Python functionality.

# Requires Docker 17.05 or higher.

# Make sure the build context is a local copy of the gromacs repository so that we can copy the source
# tree with `COPY . /tmp`. E.g.
#     docker build -f /path/to/repo/python_packaging/Dockerfile.ci_alt /path/to/repo
# or, specifically, from this directory:
#     docker build -f Dockerfile.ci_alt ..

# Optionally, set `--build-arg DOCKER_CORES=N` for a Docker engine running with access to more than 1 CPU.

# This image serves as a base for integration with the gmxapi Python tools and sample code.

FROM ubuntu as intermediate

# Basic packages
RUN apt-get update && \
    apt-get -yq --no-install-suggests --no-install-recommends install software-properties-common && \
    apt-get -yq --no-install-suggests --no-install-recommends install \
        libblas-dev \
        libcr-dev \
        libfftw3-dev \
        liblapack-dev \
        libxml2-dev \
        make \
        wget \
        zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

# The CMake available with apt-get may be too old. Get v3.13.3 binary distribution.
ENV CMAKE_ROOT /usr/local/cmake
ENV PATH $CMAKE_ROOT/bin:$PATH

RUN set -ev && \
    wget -c --no-check-certificate \
     https://github.com/Kitware/CMake/releases/download/v3.13.3/cmake-3.13.3-Linux-x86_64.tar.gz && \
    echo "78227de38d574d4d19093399fd4b40a4fb0a76cbfc4249783a969652ce515270  cmake-3.13.3-Linux-x86_64.tar.gz" \
     > cmake_sha.txt && \
    sha256sum -c cmake_sha.txt && \
    tar -xvf cmake-3.13.3-Linux-x86_64.tar.gz > /dev/null && \
    mv cmake-3.13.3-Linux-x86_64 $CMAKE_ROOT && \
    rm cmake-3.13.3-Linux-x86_64.tar.gz

RUN cmake --version

# mpich installation layer
RUN apt-get update && \
    apt-get -yq --no-install-suggests --no-install-recommends install \
        libmpich-dev \
        mpich && \
    rm -rf /var/lib/apt/lists/*

ENV SRC_DIR /tmp/gromacs-source
COPY . $SRC_DIR

ENV BUILD_DIR /tmp/gromacs-build
RUN mkdir -p $BUILD_DIR

ARG DOCKER_CORES=1
# Allow the build type to be specified with `docker build --build-arg TYPE=something`
ARG TYPE=Release
# Note: AVX2 instructions not available in older docker engines.
RUN cd $BUILD_DIR && \
    cmake $SRC_DIR \
        -DCMAKE_INSTALL_PREFIX=/usr/local/gromacs \
        -DGMXAPI=ON \
        -DGMX_THREAD_MPI=ON \
        -DGMX_BUILD_HELP=OFF \
        -DGMX_SIMD=AVX_256 \
        -DGMX_USE_RDTSCP=OFF \
        -DGMX_HWLOC=OFF \
        -DCMAKE_BUILD_TYPE=$TYPE && \
    make -j$DOCKER_CORES install


FROM ubuntu

RUN apt-get update && \
    apt-get -yq --no-install-suggests --no-install-recommends install software-properties-common && \
    apt-get -yq --no-install-suggests --no-install-recommends install \
        libblas-dev \
        libcr-dev \
        libfftw3-dev \
        liblapack-dev \
        libmpich-dev \
        libxml2-dev \
        make \
        mpich \
        zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

ENV CMAKE_ROOT /usr/local/cmake
ENV PATH $CMAKE_ROOT/bin:$PATH
COPY --from=intermediate $CMAKE_ROOT $CMAKE_ROOT
COPY --from=intermediate /usr/local/gromacs /usr/local/gromacs

RUN groupadd -r testing && useradd -r -g testing testing
USER testing

ADD --chown=testing:testing ./python_packaging/acceptance $HOME/acceptance

# MPI tests can be run in this container without requiring MPI on the host.
# We should also try tests with an MPI-connected set of docker containers.

# To be able to step through with gdb, run with something like the following, replacing
# 'imagename' with the name of the docker image built with this recipe.
# docker run --rm -ti --security-opt seccomp=unconfined imagename bash
