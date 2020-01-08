#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-build-common.sh

build_sys=ghost

env
prepare_env $build_sys


kill_http_server() {
	test "$srvpid" && kill $srvpid
	cleanup
}
trap kill_http_server EXIT


# spawn HTTP server
python3 -m http.server --bind 127.0.0.1 --directory $BUILDDIR/test-sysrepo 2>/dev/null &
srvpid=$!


#trap cleanup EXIT
cleanup
mmpack-build clean --wipe

# generate specs for the current test
gen-ghost-specs

mkdir -p $TMP_BUILD
pushd $TMP_BUILD
MMPACK_BUILD_DPKG_REPO="http://127.0.0.1:8000/debian testdata" mmpack-build
popd


# check that the packages created are correct
ls $XDG_DATA_HOME/mmpack-packages/hello*.mmpack-manifest
ls $XDG_DATA_HOME/mmpack-packages/hello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/libhello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello-devel*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello*src.tar.*
