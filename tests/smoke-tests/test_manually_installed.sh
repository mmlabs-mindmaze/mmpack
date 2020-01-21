#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

mmpack mkprefix $PREFIX_TEST
mmpack install -y hello

# check that the manually installed set contains only the package hello
diff - $PREFIX_TEST/var/lib/mmpack/manually_installed.yaml << EOF
- hello
EOF

mmpack uninstall -y hello-data

mmpack install -y hello hello-data=1.0.0

#check that the manually installed set contains the packages hello and hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually_installed.yaml | sort << EOF
- hello
- hello-data
EOF

mmpack upgrade

#check that the munually installed set contains still the packages hello and hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually_installed.yaml | sort << EOF
- hello
- hello-data
EOF

mmpack uninstall hello

#check that the manually installed set contains the package hello-data
diff - $PREFIX_TEST/var/lib/mmpack/manually_installed.yaml << EOF
- hello-data
EOF

mmpack uninstall hello

mmpack install -y hello hello-data
mmpack uninstall -y hello

#check that the manually installed set contains no package
file -b $PREFIX_TEST/var/lib/mmpack/manually_installed.yaml | grep "^empty$"



