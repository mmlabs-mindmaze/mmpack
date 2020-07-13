#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL/0 $PREFIX_TEST
mmpack repo add repo-1 $REPO_URL/1
mmpack update

diff - <(mmpack search hello | $dos2unix | sort) << EOF
[available] call-hello (1.0.0) from repositories: repo-0
[available] hello (1.0.0) from repositories: repo-0
[available] hello-data (1.0.0) from repositories: repo-0
[available] hello-data (2.0.0) from repositories: repo-1
EOF

mmpack install -y hello
diff - <(mmpack search hello | $dos2unix | sort) << EOF
[available] call-hello (1.0.0) from repositories: repo-0
[available] hello-data (1.0.0) from repositories: repo-0
[installed] hello (1.0.0) from repositories: repo-0
[installed] hello-data (2.0.0) from repositories: repo-1
EOF
