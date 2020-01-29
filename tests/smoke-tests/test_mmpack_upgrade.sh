#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name="test-repo" --url="$REPO_URL" $PREFIX_TEST
mmpack update

mmpack install hello-data=1.0.0
mmpack list installed | assert-str-equal '[installed] hello-data (1.0.0) from repositories: test-repo'

mmpack upgrade -y
mmpack list installed | assert-str-equal '[installed] hello-data (2.0.0) from repositories: test-repo'
