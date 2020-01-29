#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix $PREFIX_TEST

mmpack list all | assert-str-equal "No package found"
mmpack list available | assert-str-equal "No package found"
mmpack list installed | assert-str-equal "No package found"
mmpack list upgradeable | assert-str-equal "No package found"

mmpack install $REPO/hello-data_1.0.0*.mpk
mmpack list installed | assert-str-equal '[installed] hello-data (1.0.0) from repositories: unknown'
mmpack list all | assert-str-equal '[installed] hello-data (1.0.0) from repositories: unknown'
