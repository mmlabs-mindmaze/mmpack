#!/bin/bash

set -ex

# deal with windows environment
if [ -n "$(which cygpath)" ] ; then
	SRCDIR=$(cygpath -u $SRCDIR)
	BUILDDIR=$(cygpath -u $BUILDDIR)
	PREFIX=$(cygpath -u $PREFIX)
	DEPLOYMENT_DIR=$(cygpath -u $DEPLOYMENT_DIR)
fi

# initialisation of variables
PACKAGE=$BUILDDIR/packages
PREFIX_TEST=$BUILDDIR/prefix
CREATE=$BUILDDIR/tmp
TRASH=$BUILDDIR/trash
TEST_SRCDIR=$SRCDIR/tests/smoke-tests
_MMPACK_TEST_PREFIX=$DEPLOYMENT_DIR

# clean the files and repositories necessary for the tests
cleanup()
{
	rm -rf $PACKAGE
	rm -rf $PREFIX_TEST
}
trap cleanup EXIT

# set the environment variables for this test
name="$(basename $0)"
PREFIX_TEST+=_${name%.*}
PACKAGE+=_${name%.*}
CREATE+=_${name%.*}
TRASH+=_${name%.*}

# set the environment properly
PATH=$DEPLOYMENT_DIR$PREFIX/bin:$PATH
export MMPACK_PREFIX=$PREFIX_TEST

cleanup

# creation of a package hello.mpk
export CREATE
export PACKAGE
export TRASH
mkdir $PACKAGE
$TEST_SRCDIR/create_package_hello.sh

env

mmpack mkprefix $PREFIX_TEST
mmpack install $PACKAGE/hello*.mpk

mmpack remove hello
