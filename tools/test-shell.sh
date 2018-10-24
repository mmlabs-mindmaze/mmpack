#!/bin/bash
#
# usage:
# test-shell.sh [repository name]

distrib() {
	if [ "$(cat /etc/os-release  | grep "^ID=")" == "ID=debian" ]; then
		echo debian
	else
		echo windows
	fi
}

cd $(dirname $0)/../build

REPOSITORY=${1:-"http://mindmaze-srv-fr-01"}
DIST=$(distrib)
MMPACK_PREFIX=$(mktemp -d)

# source mmpack python virtualenv
testdir=$(pwd)/venv
python_minor=$(python3 -c 'import sys; print(sys.version_info.minor)')
python_testdir="${testdir}/lib/python3.${python_minor}"

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
export PYTHONPATH="${python_testdir}:${python_testdir}/site-packages:${PYTHONPATH}"

# create mmpack prefix
mmpack mkprefix --url="$REPOSITORY/$DIST" $MMPACK_PREFIX

# update PS1
export PS1='\u@\h:\w\$ [mmpack] '

source ../src/mmpack_completion
source ../src/mmpack-build_completion

exec < /dev/tty
EOF
} | $SHELL -i
