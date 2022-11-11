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
python3 -m http.server --bind 127.0.0.1 --directory $TESTSDIR/sysrepo $http_port 2>/dev/null &
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
mmpack-build --debug pkg-create
popd


pushd $MMPACK_BUILD_OUTDIR &> /dev/null

# check that the expected files are created
ls hello_1.0.0*.mmpack-manifest
ls hello_1.0.0*.mpk
ls libhello1_1.0.0*.mpk
ls hello-devel_1.0.0*.mpk
ls hello_1.2.3_src.tar.*


# Check that hello and hello-devel packages contains only package info and
# sumsha file, ie, they are empty
for pkgfile in hello_1.0.0*.mpk hello-devel_1.0.0*.mpk
do
	pkgname=$(echo $pkgfile | sed s/_.*//)
	cmp <(tar -tf $pkgfile) <<EOF
./
./MMPACK/
./MMPACK/info
./MMPACK/metadata
./var/
./var/lib/
./var/lib/mmpack/
./var/lib/mmpack/metadata/
./var/lib/mmpack/metadata/$pkgname.pkginfo
./var/lib/mmpack/metadata/$pkgname.sha256sums
EOF

done


# Check that libhello1 contains only package info, sumsha and symbol files
pkgfile=$(ls libhello1_1.0.0*.mpk)
pkgname=$(echo $pkgfile | sed s/_.*//)
cmp <(tar -tf $pkgfile) <<EOF
./
./MMPACK/
./MMPACK/info
./MMPACK/metadata
./var/
./var/lib/
./var/lib/mmpack/
./var/lib/mmpack/metadata/
./var/lib/mmpack/metadata/$pkgname.pkginfo
./var/lib/mmpack/metadata/$pkgname.sha256sums
./var/lib/mmpack/metadata/$pkgname.symbols.gz
EOF


popd &> /dev/null
