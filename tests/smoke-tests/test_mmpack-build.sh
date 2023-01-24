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

# generate dummy license file
echo "dummy license" > $TMP_BUILD/dummy

tar -cvf $TMP_BUILD/mmpack-hello-world.tar --directory=$SRC_PKG .
tar --update -v -f $TMP_BUILD/mmpack-hello-world.tar --directory=$TMP_BUILD mmpack/specs
tar --update -v -f $TMP_BUILD/mmpack-hello-world.tar --directory=$TMP_BUILD dummy
gzip $TMP_BUILD/mmpack-hello-world.tar
mmpack-build --debug pkg-create --src $TMP_BUILD/mmpack-hello-world.tar.gz

# check that the packages created are correct
ls $MMPACK_BUILD_OUTDIR/hello*.mmpack-manifest
ls $MMPACK_BUILD_OUTDIR/hello*.mpk
ls $MMPACK_BUILD_OUTDIR/libhello*.mpk
ls $MMPACK_BUILD_OUTDIR/hello-devel*.mpk
ls $MMPACK_BUILD_OUTDIR/hello*src.tar.*

# check that the packages contains the expected files
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep MMPACK/info
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep bin/hello-world
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep bin/head-libexec-world
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep bin/shell-exec.py
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep libexec/hello/libexec-world
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep var/lib/mmpack/metadata/hello.sha256sums
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep share/licenses/hello/dummy
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep share/licenses/hello/copyright
tar -tvf $MMPACK_BUILD_OUTDIR/hello_*.mpk | grep share/hello/.hidden/afile

tar -tvf $MMPACK_BUILD_OUTDIR/hello-devel_*.mpk | grep include/libhello.h
tar -tvf $MMPACK_BUILD_OUTDIR/hello-devel_*.mpk | grep -e lib/libhello.dll.a -e lib/libhello.so
tar -tvf $MMPACK_BUILD_OUTDIR/hello-devel_*.mpk | grep share/licenses/hello-devel/dummy
tar -tvf $MMPACK_BUILD_OUTDIR/hello-devel_*.mpk | grep share/licenses/hello-devel/copyright

tar -tvf $MMPACK_BUILD_OUTDIR/libhello*.mpk | grep -e lib/libhello.so.1.0.0 -e bin/libhello-1.dll
tar -tvf $MMPACK_BUILD_OUTDIR/libhello*.mpk | grep -P share/licenses/libhello[0-9]*/copyright
tar -tvf $MMPACK_BUILD_OUTDIR/libhello*.mpk | grep -P share/licenses/libhello[0-9]*/dummy

# check that package sumsha list expected files
sumsha=$TMP_BUILD/sha256sums
tar -x -O -f $MMPACK_BUILD_OUTDIR/hello_*.mpk ./var/lib/mmpack/metadata/hello.sha256sums > $sumsha
! grep -q MMPACK/info $sumsha
grep -q bin/hello-world $sumsha
grep -q bin/head-libexec-world $sumsha
grep -q bin/shell-exec.py $sumsha
grep -q libexec/hello/libexec-world $sumsha
! grep -q var/lib/mmpack/metadata/hello.sha256sums $sumsha
grep -q share/licenses/hello/dummy $sumsha
grep -q share/licenses/hello/copyright $sumsha
grep -q share/hello/.hidden/afile $sumsha

# check that the package files are well formed
pushd $TMP_BUILD
tar -xvf $MMPACK_BUILD_OUTDIR/hello*src.tar.*
cmp hello-world.c $SRC_PKG/hello-world.c
cmp head-libexec-world.c $SRC_PKG/head-libexec-world.c
cmp shell-exec.py $SRC_PKG/shell-exec.py
cmp libexec-world.c $SRC_PKG/libexec-world.c
popd
