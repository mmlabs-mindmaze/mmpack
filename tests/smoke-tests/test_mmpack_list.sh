#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

assert-str-equal()
{
    read rv
    rv=${rv%%[[:space:]]} # remove trailing whitespaces
    local ref=$1

    [[ "$rv" == "$ref" ]]
}

trap cleanup EXIT
cleanup

create-test-pkg


mmpack mkprefix $PREFIX_TEST

mmpack list all | assert-str-equal "No package found"
mmpack list available | assert-str-equal "No package found"
mmpack list installed | assert-str-equal "No package found"
mmpack list upgradeable | assert-str-equal "No package found"

mmpack install $PACKAGE/hello*.mpk
mmpack list installed | assert-str-equal '[installed] hello (1.0.0) from repositories: unknown'
mmpack list all | assert-str-equal '[installed] hello (1.0.0) from repositories: unknown'
