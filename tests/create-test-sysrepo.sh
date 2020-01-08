#!/bin/bash

set -e

REPO_DIR=$1
SRC_DIR=$(dirname $0)
LONG_TESTS=$2

# Skip building sysrepo if not running long tests
if ! [ "$LONG_TESTS" = "true" ]; then
	exit 0
fi


rm -rf $REPO_DIR
mkdir -p $REPO_DIR


build_debian_repo() {
	# Build debian packages
	deb_builddir=$REPO_DIR/build
	mkdir $deb_builddir
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

	# Cleanup temporary package build dirs
	rm -rf $deb_builddir
}


build_msys2_repo() {
	builddir=$(readlink -f $REPO_DIR/build)

	# Create msys2 package
	mkdir $builddir
	cp $SRC_DIR/test-packages/PKGBUILD $builddir
	pushd $SRC_DIR/test-packages/smoke &> /dev/null
	tar -cf $builddir/hello.tar.gz *
	popd &> /dev/null
	pushd $builddir &> /dev/null
	MINGW_INSTALLS=mingw64 makepkg-mingw
	popd &> /dev/null

	# Create msys2 repo
	mingw64_dir=$REPO_DIR/mingw/x86_64
	rm -rf $mingw64_dir
	mkdir -p $mingw64_dir
	cp -r $builddir/*.pkg.* $mingw64_dir
	repo-add $mingw64_dir/mingw64.db.tar.xz $builddir/*.pkg.*
}


get_host_dist() {
	local release_id

	release_id=`grep '^ID=' /etc/os-release 2> /dev/null | cut -f2- -d= | sed -e 's/\"//g'`
	[ -n "$release_id" ] && printf $release_id && return
	[ -n "$MSYSTEM" ] && printf "msys2" && return
	
	printf "unknown"
}


case $(get_host_dist) in
debian)
	build_debian_repo
	;;
msys2)
	build_msys2_repo
	;;
*)
	echo unknown distribution 2>&1
	exit 1
	;;
esac
