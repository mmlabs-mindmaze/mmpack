#!/bin/sh
#
# usage:
# test-shell.sh [repository name]

REPOSITORY=${1:-"mindmaze-srv-fr-01"}

{
cat << EOF
# generate random prefix name
export MMPACK_PREFIX=$(mktemp -d)

# system environment execution
PATH="${MMPACK_PREFIX}/bin:${PATH}"
LD_LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LD_LIBRARY_PATH}"
LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LIBRARY_PATH}"
C_INCLUDE_PATH="${MMPACK_PREFIX}/include:${C_INCLUDE_PATH}"

# source mmpack python virtualenv
. venv/bin/activate

# create mmpack prefix
mmpack mkprefix "http://$REPOSITORY"

exec < /dev/tty
EOF
} | sh -i
