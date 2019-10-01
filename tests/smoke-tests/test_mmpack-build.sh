#!/bin/bash

set -ex

env

# deal with windows environment
if [ -n "$(which cygpath)" ] ; then
	SRCDIR=$(cygpath -u $SRCDIR)
	DEPLOYMENT_DIR=$(cygpath -u $DEPLOYMENT_DIR)
	BUILDDIR=$(cygpath -u $BUILDDIR)
	PREFIX=$(cygpath -u $PREFIX)
	PYTHON_INSTALL_DIR=$(cygpath -u $PYTHON_INSTALL_DIR)
fi

export XDG_CONFIG_HOME=$BUILDDIR/mmpack-build_config
export XDG_CACHE_HOME=$BUILDDIR/mmpack-build_cache
export XDG_DATA_HOME=$BUILDDIR/mmpack-build_data
SRC_PKG=$SRCDIR/tests/smoke-test
export _MMPACK_TEST_PREFIX=$DEPLOYMENT_DIR

# update the environment variables
PATH=$DEPLOYMENT_DIR$PREFIX/bin:$PATH

export PYTHONPATH=${DEPLOYMENT_DIR}${PYTHON_INSTALL_DIR}

mmpack-build clean --wipe

# test the creation of package
tar -cvzf $BUILDDIR/mmpack-hello-world.tar.gz --directory=$SRC_PKG .
mmpack-build pkg-create --src $BUILDDIR/mmpack-hello-world.tar.gz
