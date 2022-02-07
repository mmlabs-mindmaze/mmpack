#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL/0 $PREFIX_TEST
mmpack repo add repo-1 $REPO_URL/1
mmpack update

mmpack source hello-data
ls $PREFIX_TEST/src/hello-data-2.0.0-*
