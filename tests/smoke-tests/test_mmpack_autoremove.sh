#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL $PREFIX_TEST
mmpack update
mmpack install -y hello

mmpack autoremove -y
# check that autoremove has not suppressed hello-data that is needed by hello
diff - <(mmpack list installed | $dos2unix) << EOF
[installed] hello (1.0.0) from repositories: repo-0
[installed] hello-data (2.0.0) from repositories: repo-0
EOF

mmpack uninstall hello

mmpack list installed | assert-str-equal '[installed] hello-data (2.0.0) from repositories: repo-0'

mmpack autoremove -y
#check that autoremove has suppressed hello-data
mmpack list installed | assert-str-equal "No package found"
