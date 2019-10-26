#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

create-test-pkg
env


mmpack mkprefix $PREFIX_TEST
mmpack install $PACKAGE/hello*.mpk

mmpack remove hello
