#!/bin/sh
#
# usage:
# test-shell.sh [repository name]

REPOSITORY=${1:-"http://mindmaze-srv-fr-01"}
MMPACK_PREFIX=$(mktemp -d)

{
cat << EOF
# generate random prefix name
export MMPACK_PREFIX=${MMPACK_PREFIX}

# system environment execution
export PATH="${MMPACK_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LD_LIBRARY_PATH}"
export LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LIBRARY_PATH}"
export CPATH="${MMPACK_PREFIX}/include:${CPATH}"

# source mmpack python virtualenv
. venv/bin/activate

# create mmpack prefix
mmpack mkprefix "$REPOSITORY"

exec < /dev/tty
EOF
} | $SHELL -i
