#!/bin/sh

set -e

DSTDIR=$1
SRCDIR=$(dirname $0)/test-packages

if [ -n "$(which cygpath)" ] ; then
    SRCDIR=$(cygpath -u $SRCDIR)
    DSTDIR=$(cygpath -u $DSTDIR)
    ACLOCAL_PATH=$(cygpath -u $ACLOCAL_PATH)
fi

LONG_TESTS=$2

mkdir -p $(dirname $DSTDIR)
cp -a $SRCDIR $DSTDIR

if [ "$LONG_TESTS" = "true" ]; then
	autoreconf -fi $DSTDIR/smoke 2>&1
fi
