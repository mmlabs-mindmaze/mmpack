#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name=repo --url=$REPO_URL $PREFIX_TEST
mmpack update

output="$(mmpack rdepends toto | $dos2unix)"
expected="No package toto (any version)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --repo=repo hello | $dos2unix)"
expected="call-hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends call-hello | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive --repo=repo call-hello | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --repo=repo hello-data=1.0.0 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive --repo=repo hello-data | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=1.0.0 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]
