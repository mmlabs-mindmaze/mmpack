#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL $PREFIX_TEST
mmpack install -y $REPO/hello_1.0.0.mpk $REPO/hello-data_1.0.0.mpk

$PREFIX_TEST/bin/hello-world

mmpack uninstall hello hello-data
mmpack update

# test filtering on a specific repository
diff - <(mmpack install --repo=non_existing_repo hello-data | $dos2unix) << EOF
Repository non_existing_repo not found
EOF

echo "on est là"

mmpack install -y --repo=repo-0 hello-data hello
$PREFIX_TEST/bin/hello-world
