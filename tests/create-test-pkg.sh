#!/bin/sh

set -e

DSTDIR=$1
SRCDIR=$(dirname $0)/test-packages

LONG_TESTS=$2

mkdir -p $(dirname $DSTDIR)
cp -a $SRCDIR $DSTDIR

if [ "$LONG_TESTS" = "true" ]; then
	autoreconf -fi $DSTDIR/smoke
fi
