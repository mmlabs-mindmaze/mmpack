#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name=repo --url=$REPO_URL/0 $PREFIX_TEST
mmpack update

output="$(mmpack rdepends toto | $dos2unix)"
expected="No package toto"
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

output="$(mmpack rdepends hello-data=hash:8dc8f06e9277b4404661045a43d31dbc0f85bebd5f26e29a801a0efb12d064b6 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=hash:8dc8f06e9277b4404661045a43d31dbc0f85bebd5f26e29a801a0efb12d064b6 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive toto=hash:0000 | $dos2unix)"
expected="No package toto respecting the constraints"
[ "$output" == "$expected" ]

