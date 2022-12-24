#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

unset MMPACK_PREFIX
prefix1=$PREFIX_TEST/first
prefix2=$PREFIX_TEST/second

mmpack -p $prefix1 mkprefix --name="test-repo" --url="$REPO_URL/0"
mmpack -p $prefix1 update
mmpack -p $prefix2 mkprefix --name="test-repo" --url="$REPO_URL/0"
mmpack -p $prefix2 update

mmpack -p $prefix1 install -y hello
mmpack -p $prefix2 install -y hello

check_file_match() {
	inode1=$(stat -c %i $prefix1/$1)
	inode2=$(stat -c %i $prefix2/$1)
	test "$inode1" = "$inode2"
}

check_file_match bin/hello-world
check_file_match var/lib/mmpack/metadata/hello.sha256sums
