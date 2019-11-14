#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-build-common.sh

build_sys=$1

env
prepare_env $build_sys

#trap cleanup EXIT
cleanup
mmpack-build clean --wipe

# generate specs for the current test
gen-specs $build_sys

mkdir -p $TMP_BUILD
tar -cvf $TMP_BUILD/mmpack-hello-world.tar --directory=$SRC_PKG .
tar --update -v -f $TMP_BUILD/mmpack-hello-world.tar --directory=$TMP_BUILD mmpack/specs
gzip $TMP_BUILD/mmpack-hello-world.tar
mmpack-build pkg-create --src $TMP_BUILD/mmpack-hello-world.tar.gz

# check that the packages created are correct
ls $XDG_DATA_HOME/mmpack-packages/hello*.mmpack-manifest
ls $XDG_DATA_HOME/mmpack-packages/hello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/libhello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello-devel*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello*src.tar.*

# check that the packages contains the expected files
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep MMPACK/info
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep bin/hello-world
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep bin/head-libexec-world
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep bin/shell-exec.py
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep libexec/hello/libexec-world
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello_*.mpk | grep var/lib/mmpack/metadata/hello.sha256sums

tar -tvf $XDG_DATA_HOME/mmpack-packages/hello-devel_*.mpk | grep include/libhello.h
tar -tvf $XDG_DATA_HOME/mmpack-packages/hello-devel_*.mpk | grep -e lib/libhello.dll.a -e lib/libhello.so

tar -tvf $XDG_DATA_HOME/mmpack-packages/libhello*.mpk | grep -e lib/libhello.so.1.0.0 -e bin/libhello-1.dll

# check that the package files are well formed
pushd $TMP_BUILD
tar -xvf $XDG_DATA_HOME/mmpack-packages/hello*src.tar.*
cmp hello-world.c $SRC_PKG/hello-world.c
cmp head-libexec-world.c $SRC_PKG/head-libexec-world.c
cmp shell-exec.py $SRC_PKG/shell-exec.py
cmp libexec-world.c $SRC_PKG/libexec-world.c
popd
