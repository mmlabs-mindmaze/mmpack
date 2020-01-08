#!/bin/bash

set -e

REPO_DIR=$1
SRC_DIR=$(dirname $0)
LONG_TESTS=$2

# Skip building sysrepo if not running long tests
if ! [ "$LONG_TESTS" = "true" ]; then
	exit 0
fi

# Build debian packages
deb_builddir=$REPO_DIR/build
mkdir -p $deb_builddir
cp -a $SRC_DIR/test-packages/smoke $deb_builddir
pushd $deb_builddir/smoke &> /dev/null
dpkg-buildpackage --no-sign &> /dev/null
popd &> /dev/null


# Create a simple APT repo
rm -rf $REPO_DIR/debian
pooldir=$REPO_DIR/debian/pool
distdir=$REPO_DIR/debian/dists/testdata
mkdir -p $pooldir
mkdir -p $distdir/binary-amd64
for debfile in $deb_builddir/*.deb
do
	dpkg-deb -f $debfile dpkg-deb -f $debfile Package Source Version Architecture Maintainer Installed-Size Depends Section Description
	cp $debfile $pooldir
	echo "Filename: pool/$(basename $debfile)"
	echo ""
done | gzip > $distdir/binary-amd64/Packages.gz


rm -rf $deb_builddir
