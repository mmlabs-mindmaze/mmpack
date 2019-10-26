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

# test the executables installed
$PREFIX_TEST/bin/dum.sh
$PREFIX_TEST/bin/dummy.sh || echo "failed as expected"
