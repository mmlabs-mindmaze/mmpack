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

output="$(mmpack rdepends hello-data=hash:de87874afd40866d71a7c899ca488f1c3626e2c2d6cdc1f2bcf86c72d8ce4d02 | $dos2unix)"
expected="hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive hello-data=hash:de87874afd40866d71a7c899ca488f1c3626e2c2d6cdc1f2bcf86c72d8ce4d02 | $dos2unix)"
expected="call-hello (1.0.0)
hello (1.0.0)"
[ "$output" == "$expected" ]

output="$(mmpack rdepends --recursive toto=hash:0000 | $dos2unix)"
expected="No package toto respecting the constraints"
[ "$output" == "$expected" ]

