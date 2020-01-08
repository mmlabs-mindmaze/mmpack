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


get_unused_port() {
	local port
	port=8000
	while netstat -atn | grep -q :$port
	do
		port=$(expr $port + 1)
	done
	echo $port
}


# spawn HTTP server
http_port=$(get_unused_port)
python3 -m http.server --bind 127.0.0.1 --directory $BUILDDIR/test-sysrepo $http_port 2>/dev/null &
srvpid=$!


#trap cleanup EXIT
cleanup
mmpack-build clean --wipe

# generate specs for the current test
gen-ghost-specs

mkdir -p $TMP_BUILD
pushd $TMP_BUILD
export MMPACK_BUILD_DPKG_REPO="http://127.0.0.1:$http_port/debian testdata"
export MMPACK_BUILD_MSYS2_REPO="http://127.0.0.1:$http_port"
mmpack-build --debug
popd


# check that the packages created are correct
ls $XDG_DATA_HOME/mmpack-packages/hello*.mmpack-manifest
ls $XDG_DATA_HOME/mmpack-packages/hello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/libhello*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello-devel*.mpk
ls $XDG_DATA_HOME/mmpack-packages/hello*src.tar.*
