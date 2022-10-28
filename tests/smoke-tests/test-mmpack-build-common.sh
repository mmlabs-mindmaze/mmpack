#!/bin/bash

shopt -s expand_aliases

prepare_env()
{
	local build_sys=$1

	# make sure we don't use any mmpack prefix from the user
	# environment. We should not need any.
	unset MMPACK_PREFIX

	if [ -n "$(which cygpath)" ] ; then
	    SRCDIR=$(cygpath -u $SRCDIR)
	    BUILDDIR=$(cygpath -u $BUILDDIR)
	fi

	export TMP_BUILD=$BUILDDIR/tmp-build_$build_sys
	export XDG_CONFIG_HOME=$BUILDDIR/config_$build_sys
	export XDG_CACHE_HOME=$BUILDDIR/cache_$build_sys
	export MMPACK_BUILD_OUTDIR=$BUILDDIR/data_$build_sys
	SRC_PKG=$BUILDDIR/test-packages/smoke/

	export PATH=$BUILDDIR/src/mmpack_build:$PATH

	alias mmpack-build=$BUILDDIR/src/mmpack_build/mmpack-build$EXEEXT
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
description: mmpack hello. 你好世界. This package is a test
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
