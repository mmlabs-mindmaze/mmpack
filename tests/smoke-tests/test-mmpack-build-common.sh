#!/bin/bash

prepare_env()
{
	local build_sys=$1

	if [ -n "$(which cygpath)" ] ; then
	    SRCDIR=$(cygpath -u $SRCDIR)
	    _MMPACK_TEST_PREFIX=$(cygpath -u $_MMPACK_TEST_PREFIX)
	    BUILDDIR=$(cygpath -u $BUILDDIR)
	    PREFIX=$(cygpath -u $PREFIX)
	    PYTHON_INSTALL_DIR=$(cygpath -u $PYTHON_INSTALL_DIR)
	fi

	export TMP_BUILD=$BUILDDIR/tmp-build_$build_sys
	export XDG_CONFIG_HOME=$BUILDDIR/config_$build_sys
	export XDG_CACHE_HOME=$BUILDDIR/cache_$build_sys
	export XDG_DATA_HOME=$BUILDDIR/data_$build_sys
	export PYTHONPATH=${_MMPACK_TEST_PREFIX}${PYTHON_INSTALL_DIR}
	SRC_PKG=$SRCDIR/tests/smoke-test

	export PATH=$_MMPACK_TEST_PREFIX$PREFIX/bin:$PATH
}

cleanup()
{
    rm -rf $SRC_PKG/mmpack/specs

    rm -rf $XDG_CONFIG_HOME
    rm -rf $XDG_CACHE_HOME
    rm -rf $XDG_DATA_HOME
    rm -rf $TMP_BUILD
}

gen-specs()
{
	local build_sys=$1

mkdir -p $TMP_BUILD/mmpack

cat << EOF > $TMP_BUILD/mmpack/specs
general:
  name: hello
  version: 1.0.0
  maintainer: maintainer
  url: url
  description: mmpack hello
  build-system : $build_sys
EOF
}
