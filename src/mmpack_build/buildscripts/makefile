# vi:syntax=sh

make -q test > /dev/null 2> /dev/null
has_test_target=$(($? == 1))

make -q check > /dev/null 2> /dev/null
has_check_target=$(($? == 1))

set -e

export CFLAGS="-flto -g -gno-record-gcc-switches -O2 -ffile-prefix-map=${SRCDIR}=${INSTALLED_SRCDIR}"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="-Wl,-rpath-link=$PREFIX/lib  -Wl,--build-id -flto"

cd $SRCDIR
eval make $OPTS PREFIX=$PREFIX

if [ "$SKIP_TESTS" != "True" ] ; then
	ld_lib_path=$PREFIX/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}

	if [ $has_check_target -eq 1 ] ; then
		LD_LIBRARY_PATH=$ld_lib_path eval make check $OPTS PREFIX=$PREFIX
	elif [ $has_test_target -eq 1 ] ; then
		LD_LIBRARY_PATH=$ld_lib_path eval make test $OPTS PREFIX=$PREFIX
	else
		echo "No test target found" 1>&2
		exit 1
	fi
fi

eval make install $OPTS PREFIX=$PREFIX DESTDIR=$DESTDIR
