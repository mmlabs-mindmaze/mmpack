#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

# test that the tree of files from $PREFIX_TEST is the one expected
diff_tree()
{
	if [ $# -eq 1 ]
	then
		diff <(find $PREFIX_TEST -type f | sort) <(cat <<EOF
$PREFIX_TEST/etc/mmpack-config.yaml
$1
$PREFIX_TEST/var/lib/mmpack/installed.yaml
$PREFIX_TEST/var/log/mmpack.log
EOF
)
	else
		diff <(find $PREFIX_TEST -type f | sort) <(cat <<EOF
$PREFIX_TEST/etc/mmpack-config.yaml
$PREFIX_TEST/var/lib/mmpack/installed.yaml
$PREFIX_TEST/var/log/mmpack.log
EOF
)
	fi
}

tests_tree_and_files()
{
	diff_tree $1
	if [ $# -eq 1 ]
	then
		file -b $1 | grep "^empty$"
	fi
	file -b $PREFIX_TEST/var/lib/mmpack/installed.yaml | grep "^empty$"
}


# test mkprefix without url and name
mmpack mkprefix $PREFIX_TEST

tests_tree_and_files

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat <<EOF
repositories:
EOF
)

cleanup

# test mkprefix with url but no name
mmpack mkprefix --url=my_url $PREFIX_TEST

tests_tree_and_files $PREFIX_TEST/var/lib/mmpack/binindex.yaml.repo-0

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat <<EOF
repositories:
  - repo-0:
        url: my_url
        enabled: 1
EOF
)

cleanup

# test mkprefix with url and name
mmpack mkprefix --name=my_name --url=my_url $PREFIX_TEST

tests_tree_and_files $PREFIX_TEST/var/lib/mmpack/binindex.yaml.my_name

diff $PREFIX_TEST/etc/mmpack-config.yaml <(cat <<EOF
repositories:
  - my_name:
        url: my_url
        enabled: 1
EOF
)
