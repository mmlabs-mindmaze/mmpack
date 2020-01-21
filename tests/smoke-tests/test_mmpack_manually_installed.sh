#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix --url=$REPO_URL $PREFIX_TEST
mmpack update
mmpack install -y hello

# check that the manually installed set contains only the package hello
diff - $PREFIX_TEST/var/lib/mmpack/manually-installed.txt << EOF
hello
EOF

mmpack uninstall -y hello-data

mmpack install hello hello-data=1.0.0

#check that the manually installed set contains the packages hello and hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually-installed.txt << EOF
hello-data
hello
EOF

mmpack upgrade -y

#check that the munually installed set contains still the packages hello and hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually-installed.txt << EOF
hello-data
hello
EOF

mmpack uninstall hello

#check that the manually installed set contains the package hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually-installed.txt << EOF
hello-data
EOF

mmpack uninstall hello-data
mmpack install hello hello-data
mmpack uninstall -y hello-data

#check that the manually installed set contains no package
file -b $PREFIX_TEST/var/lib/mmpack/manually-installed.txt | grep "^empty$"
