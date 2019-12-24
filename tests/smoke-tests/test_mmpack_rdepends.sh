#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --name=repo --url=$REPO_URL $PREFIX_TEST
mmpack update

output="$(mmpack rdepends toto | $dos2unix)"
expected="Package toto not found"
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello=fromrepo:repo | $dos2unix)"
expected="call-hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends call-hello | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive call-hello=fromrepo:repo | $dos2unix)"
expected=""
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data=1.0.0fromrepo:repo | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=fromrepo:repo | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=1.0.0 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends hello-data=sumsha:9f68904c9c1761388382afdb7d7b7618101e353b5b93b872b10cc71f0a7c8a34 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=sumsha:8dc8f06e9277b4404661045a43d31dbc0f85bebd5f26e29a801a0efb12d064b6 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive toto=sumsha:0000 | $dos2unix)"
expected="Package toto not found"
[ "$output" == "$expected" ]

