
prepare_env()
{
	# deal with windows environment
	if [ -n "$(which cygpath)" ] ; then
		SRCDIR=$(cygpath -u $SRCDIR)
		BUILDDIR=$(cygpath -u $BUILDDIR)
		PREFIX=$(cygpath -u $PREFIX)
		_MMPACK_TEST_PREFIX=$(cygpath -u $_MMPACK_TEST_PREFIX)
	fi

	# initialisation of variables
	PACKAGE=$BUILDDIR/packages
	PREFIX_TEST=$BUILDDIR/prefix
	CREATE=$BUILDDIR/tmp
	TRASH=$BUILDDIR/trash
	TEST_SRCDIR=$SRCDIR/tests/smoke-tests

	# set the environment variables for this test
	name="$(basename $0)"
	PREFIX_TEST+=_${name%.*}
	export PACKAGE+=_${name%.*}
	export CREATE+=_${name%.*}
	export TRASH+=_${name%.*}

	# set the environment properly
	PATH=$_MMPACK_TEST_PREFIX$PREFIX/bin:$PATH
	export MMPACK_PREFIX=$PREFIX_TEST

	# prevent loading user global configuration
	export XDG_CONFIG_HOME=/non-existing-dir
}

# clean the files and repositories necessary for the tests
cleanup()
{
	rm -rf $PACKAGE
	rm -rf $PREFIX_TEST
}

create-test-pkg()
{
	# creation of a package hello.mpk
	mkdir $PACKAGE
	$TEST_SRCDIR/create_package_hello.sh
}
