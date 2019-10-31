#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix $PREFIX_TEST
mmpack install $REPO/hello-data_1.0.0*.mpk

mmpack remove hello-data
