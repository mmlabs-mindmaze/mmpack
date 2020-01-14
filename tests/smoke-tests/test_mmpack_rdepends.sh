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

output="$(mmpack rdepends hello=repo:repo | $dos2unix)"
expected="call-hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends call-hello | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive call-hello=repo:repo | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data=repo:repo,1.0.0 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=repo:repo | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=1.0.0 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data=hash:9f68904c9c1761388382afdb7d7b7618101e353b5b93b872b10cc71f0a7c8a34 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=hash:8dc8f06e9277b4404661045a43d31dbc0f85bebd5f26e29a801a0efb12d064b6 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive toto=hash:0000 | $dos2unix)"
expected="No package toto (any version)"
[ "$output" == "$expected" ]

