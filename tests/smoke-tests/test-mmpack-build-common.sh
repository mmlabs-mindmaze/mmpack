#!/bin/bash

prepare_env()
{
	local build_sys=$1

	# make sure we don't use any mmpack prefix from the user
	# environment. We should not need any.
	unset MMPACK_PREFIX

	if [ -n "$(which cygpath)" ] ; then
	    SRCDIR=$(cygpath -u $SRCDIR)
	    _MMPACK_TEST_PREFIX=$(cygpath -u $_MMPACK_TEST_PREFIX)
	    BUILDDIR=$(cygpath -u $BUILDDIR)
	    PREFIX=$(cygpath -u $PREFIX)
	    PKGDATADIR=$(cygpath -u $PKGDATADIR)
	fi

	export TMP_BUILD=$BUILDDIR/tmp-build_$build_sys
	export XDG_CONFIG_HOME=$BUILDDIR/config_$build_sys
	export XDG_CACHE_HOME=$BUILDDIR/cache_$build_sys
	export MMPACK_BUILD_OUTDIR=$BUILDDIR/data_$build_sys
	export PYTHONPATH=${_MMPACK_TEST_PREFIX}${PKGDATADIR}
	SRC_PKG=$BUILDDIR/test-packages/smoke/

	export PATH=$_MMPACK_TEST_PREFIX$PREFIX/bin:$PATH
}

cleanup()
{
    rm -rf $SRC_PKG/mmpack/specs

    rm -rf $XDG_CONFIG_HOME
    rm -rf $XDG_CACHE_HOME
    rm -rf $MMPACK_BUILD_OUTDIR
    rm -rf $TMP_BUILD
}

gen-specs()
{
	local build_sys=$1

mkdir -p $TMP_BUILD/mmpack

cat << EOF > $TMP_BUILD/mmpack/specs
name: hello
version: 1.0.0
maintainer: maintainer
url: url
description: mmpack hello
build-system : $build_sys
licenses: [ dummy ]
copyright: "dummy copyright"
EOF
}

gen-ghost-specs()
{
mkdir -p $TMP_BUILD/mmpack

cat << EOF > $TMP_BUILD/mmpack/specs
name: hello
version: 1.2.3
maintainer: maintainer
url: url
description: mmpack hello
ghost: true
EOF
}
