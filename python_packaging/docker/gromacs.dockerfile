# Provide an easy-to-reproduce environment in which to test full Python functionality.
# Produce an image with GROMACS installed.
# This version of the Dockerfile installs mpich.

# Optionally, set `--build-arg DOCKER_CORES=N` for a Docker engine running with access to more than 1 CPU.
#    REF=`git show -s --pretty=format:"%h"`
#    docker build -t gmxapi/gromacs:${REF} --build-arg REF=${REF} --build-arg DOCKER_CORES=4 -f gromacs.dockerfile ..

# This image serves as a base for integration with the gmxapi Python tools and sample code.

ARG MPIFLAVOR=mpich
FROM gmxapi/gromacs-dependencies:$MPIFLAVOR

#ARG REPO=https://github.com/gromacs/gromacs.git
ARG REPO=https://github.com/eirrgang/gromacs-gmxapi.git

RUN cd /tmp && git clone $REPO gromacs-source
ENV SRC_DIR /tmp/gromacs-source

# Allow build for an arbitrary branch, tag, or commit, but default to the tip of `master`
ARG REF=master
# NOTE! If a branch is used for REF, the command looks the same and Docker will not expire the
# build cache after success of the following command. Consider giving a specific commit for REF
# or specify `--no-cache` if a rebuild requires updates from the git repository.
RUN cd $SRC_DIR && \
    git pull && \
    git checkout $REF

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

