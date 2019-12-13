
prepare_env()
{
	# deal with windows environment
	if [ -n "$(which cygpath)" ] ; then
		SRCDIR=$(cygpath -u $SRCDIR)
		BUILDDIR=$(cygpath -u $BUILDDIR)
		PREFIX=$(cygpath -u $PREFIX)
		_MMPACK_TEST_PREFIX=$(cygpath -u $_MMPACK_TEST_PREFIX)

		# get the windows-format of the full path to $REPO
		# it will be used with curl with the file protocol
		# and needs to be in absolute native format
		REPO_URL="file://$(cygpath -m $REPO)"
		dos2unix=dos2unix
	else
		REPO_URL="file://$REPO"
		dos2unix=cat
	fi

	# initialisation of variables
	PREFIX_TEST=$BUILDDIR/prefix
	TEST_SRCDIR=$SRCDIR/tests/smoke-tests

	# set the environment variables for this test
	name="$(basename $0)"
	PREFIX_TEST+=_${name%.*}

	# set the environment properly
	PATH=$_MMPACK_TEST_PREFIX$PREFIX/bin:$PATH
	export MMPACK_PREFIX=$PREFIX_TEST

	# prevent loading user global configuration
	export XDG_CONFIG_HOME=/non-existing-dir
}

# clean the files and repositories necessary for the tests
cleanup()
{
	rm -rf $PREFIX_TEST
}
