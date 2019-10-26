#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

create-test-pkg


mmpack mkprefix $PREFIX_TEST
mmpack install $PACKAGE/hello*.mpk
mmpack show hello | grep -q 'hello (1.0.0) \[installed\]'
