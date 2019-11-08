#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix $PREFIX_TEST
mmpack install $REPO/hello-data_1.0.0*.mpk

mmpack check-integrity

# compromized integrity detection works
# => try to fix
# this needs a repository containing the good package
cleanup

mmpack mkprefix --url=$REPO_URL $PREFIX_TEST
mmpack update
mmpack install -y hello

$PREFIX_TEST/bin/hello-world

echo "" > $PREFIX_TEST/bin/hello-world

mmpack check-integrity && false || echo "Fail as expected"

mmpack fix-broken hello

# should be good now
mmpack check-integrity

# should not do anything
tree_before=$(find $PREFIX_TEST -type f | sort | sha1sum)
mmpack fix-broken
tree_after=$(find $PREFIX_TEST -type f | sort | sha1sum)

[ "$tree_before" == "$tree_after" ]
