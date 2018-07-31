#!/bin/bash
set -ev

SOURCEDIR=$HOME/mpich-3.2.1
BUILDDIR=/tmp/mpich-build
INSTALLDIR=$MPICH_DIR

rm -rf $SOURCEDIR
rm -rf $BUILDDIR
rm -rf $INSTALLDIR

${C_COMPILER} --version
${CXX_COMPILER} --version

pushd $HOME
    wget http://www.mpich.org/static/downloads/3.2.1/mpich-3.2.1.tar.gz
    tar zxf mpich-3.2.1.tar.gz
    mkdir $BUILDDIR
    pushd $BUILDDIR
        mkdir $INSTALLDIR
        $SOURCEDIR/configure \
            --prefix=$INSTALLDIR \
            --enable-shared \
            --disable-fortran \
            CC=`which ${C_COMPILER}` \
            CXX=`which ${CXX_COMPILER}`
        make -j2 install
    popd
popd
