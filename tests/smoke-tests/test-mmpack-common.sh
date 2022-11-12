shopt -s expand_aliases

prepare_env()
{
	# deal with windows environment
	if [ -n "$(which cygpath)" ] ; then
		SRCDIR=$(cygpath -u $SRCDIR)
		BUILDDIR=$(cygpath -u $BUILDDIR)
		TESTSDIR=$(cygpath -u $TESTSDIR)

		# get the windows-format of the full path to $REPO
		# it will be used with curl with the file protocol
		# and needs to be in absolute native format
		REPO_URL="file://$(cygpath -m $REPO)"
		dos2unix=dos2unix
	else
		REPO_URL="file://$REPO"
		dos2unix=cat
	fi

	alias mmpack=$BUILDDIR/src/mmpack/mmpack$EXEEXT

	# initialisation of variables
	PREFIX_TEST=$TESTSDIR/prefix
	TEST_SRCDIR=$SRCDIR/tests/smoke-tests

	# set the environment variables for this test
	name="$(basename $0)"
	PREFIX_TEST+=_${name%.*}

	# set the environment properly
	export MMPACK_PREFIX=$PREFIX_TEST
	export XDG_CACHE_HOME=$TESTSDIR/xdg-cache
	export TESTS_DATA_DIR=$TESTSDIR/tmp-data

	# prevent loading user global configuration
	export XDG_CONFIG_HOME=/non-existing-dir
}

# clean the files and repositories necessary for the tests
cleanup()
{
	rm -rf $PREFIX_TEST
	rm -rf $TESTS_DATA_DIR
}

assert-str-equal()
{
    read rv
    rv=${rv%%[[:space:]]} # remove trailing whitespaces
    local ref=$1

    [[ "$rv" == "$ref" ]]
}

