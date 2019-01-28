#!/bin/bash
#
# usage:
# test-shell.sh [repository name]

distrib() {
	if grep -q "^ID=debian" /etc/os-release ; then
		echo debian
	else
		echo windows
	fi
}

cd $(dirname $0)/../build

REPOSITORY=${1:-"http://mindmaze-srv-fr-01"}
DIST=$(distrib)
MMPACK_PREFIX=$(mktemp -d)

# local install to test prefix
testdir=$(pwd)/local-install
make install prefix=$testdir
[[ $? -eq 0 ]] || exit -1
if [ $DIST == "debian" ] ; then
	sudo make setcap prefix=$testdir
	[[ $? -eq 0 ]] || exit -1
fi

{
cat << EOF
# generate random prefix name
export MMPACK_PREFIX=${MMPACK_PREFIX}

# system environment execution
export PATH="${testdir}/bin:${MMPACK_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LD_LIBRARY_PATH}"
export LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LIBRARY_PATH}"
export CPATH="${MMPACK_PREFIX}/include:${CPATH}"

export VIRTUAL_ENV=${testdir}

# create mmpack prefix
mmpack mkprefix --url="$REPOSITORY/$DIST" $MMPACK_PREFIX

# update PS1
export PS1='\u@\h:\w\$ [mmpack] '

source ../src/mmpack/mmpack_completion
source ../src/mmpack-build/mmpack-build_completion

exec < /dev/tty
EOF
} | $SHELL -i

# clean to remove the prefix value used in the local install
rm -rf $testdir
make clean
