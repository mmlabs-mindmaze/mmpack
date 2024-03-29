#!/bin/bash

set -ex

. $(dirname $0)/test-mmpack-common.sh
prepare_env

trap cleanup EXIT
cleanup

# test that the tree of files from $PREFIX_TEST is the one expected
diff_tree()
{
	if [ $# -eq 2 ]
	then
		diff - <(find $PREFIX_TEST -type f | grep -v __pycache__ | sort) <<EOF
$PREFIX_TEST/etc/mmpack-config.yaml
$PREFIX_TEST/lib/python3/site-packages/site.py
$1
$PREFIX_TEST/var/lib/mmpack/installed
$PREFIX_TEST/var/lib/mmpack/manually-installed.txt
$2
$PREFIX_TEST/var/log/mmpack.log
EOF
	else
		diff - <(find $PREFIX_TEST -type f | grep -v __pycache__ | sort) <<EOF
$PREFIX_TEST/etc/mmpack-config.yaml
$PREFIX_TEST/lib/python3/site-packages/site.py
$PREFIX_TEST/var/lib/mmpack/installed
$PREFIX_TEST/var/lib/mmpack/manually-installed.txt
$PREFIX_TEST/var/log/mmpack.log
EOF
	fi
}

tests_tree_and_files()
{
	diff_tree $1 $2
	if [ $# -eq 2 ]
	then
		file -b $1 | grep "^empty$"
		file -b $2 | grep "^empty$"
	fi
	file -b $PREFIX_TEST/var/lib/mmpack/installed | grep "^empty$"
	file -b $PREFIX_TEST/var/lib/mmpack/manually-installed.txt | grep "^empty$"
}


# test mkprefix without url and name
mmpack mkprefix $PREFIX_TEST

tests_tree_and_files

diff - $PREFIX_TEST/etc/mmpack-config.yaml <<EOF
repositories:
EOF

cleanup

# test mkprefix with url but no name
mmpack mkprefix --url=my_url $PREFIX_TEST

tests_tree_and_files $PREFIX_TEST/var/lib/mmpack/binindex.repo-0 $PREFIX_TEST/var/lib/mmpack/srcindex.repo-0

diff - $PREFIX_TEST/etc/mmpack-config.yaml <<EOF
repositories:
  - repo-0:
        url: my_url
        enabled: 1
EOF

cleanup

# test mkprefix with url and name
mmpack mkprefix --name=my_name --url=my_url $PREFIX_TEST

tests_tree_and_files $PREFIX_TEST/var/lib/mmpack/binindex.my_name $PREFIX_TEST/var/lib/mmpack/srcindex.my_name

diff - $PREFIX_TEST/etc/mmpack-config.yaml <<EOF
repositories:
  - my_name:
        url: my_url
        enabled: 1
EOF

cleanup

# test mkprefix with --prefix
mmpack --prefix=$PREFIX_TEST mkprefix
tests_tree_and_files

cleanup

# test mkprefix with conflicting --prefix
mmpack --prefix=my_name mkprefix $PREFIX_TEST
tests_tree_and_files

cleanup

# test mkprefix with invalid arguments
! mmpack mkprefix tata tito tutu
! mmpack --prefix=my_name mkprefix tata tito tutu
