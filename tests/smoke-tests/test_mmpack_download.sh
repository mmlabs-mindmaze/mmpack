#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL/0 $PREFIX_TEST
mmpack update

mkdir -p $TESTS_DATA_DIR
pushd $TESTS_DATA_DIR
mmpack download hello-data=1.0.0
ls hello-data_1.0.0.mpk
popd
