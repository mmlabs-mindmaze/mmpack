#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix $PREFIX_TEST
mmpack install -y $REPO/0/hello_1.0.0.mpk $REPO/0/hello-data_1.0.0.mpk

$PREFIX_TEST/bin/hello-world
