#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL $PREFIX_TEST
mmpack update

mmpack download hello-data=1.0.0
ls hello-data_1.0.0.mpk
