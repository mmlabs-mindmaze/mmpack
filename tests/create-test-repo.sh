#!/bin/bash

set -e

REPO_DIR=$1
pkg_builddir=$REPO_DIR/build
arch=unknown


if [ -n "$(which cygpath)" ] ; then
	REPO_DIR=$(cygpath -u $REPO_DIR)
fi

# prepend src folder to PYTHONPATH
srcdir=$(readlink -f $(dirname $0)/../src)
PYTHONPATH="$srcdir${PYTHONPATH:+:${PYTHONPATH}}"

modifyrepo="python3 -m repository"


cleanup()
{
	rm -rf $pkg_builddir
}
trap cleanup EXIT

prepare()
{
	cleanup

	mkdir -p $pkg_builddir/pkgs
	mkdir -p $pkg_builddir/tmp/bin

	mkdir -p $pkg_builddir/tmp/MMPACK
	mkdir -p $pkg_builddir/tmp/var/lib/mmpack/metadata/
}

gen-src-archive()
{
	local pkgname=$1
	local version="$2"

	tar --mtime='1970-01-01' --owner=0 --group=0 --numeric-owner --sort=name -cJf pkgs/${pkgname}_${version}_src.tar.xz --directory=tmp .
}

sha256()
{
	sha256sum $1 | cut -d$' ' -f1
}

gen-mmpack-pkginfo()
{
	local pkgname="$1"
	local version="$2"
	shift 2
	local depends="$@"

	local depends=""
	for dep in "$@"; do
		[ -z "$dep" ] && continue
		depname=$(echo $dep | cut -f1 -d' ')
		depop=$(echo $dep | cut -f2 -d' ')
		depversion=$(echo $dep | cut -f3 -d' ')

		currdepend="$depname ($depop $depversion)"
		if [ -z "$depends" ]; then
			depends=$currdepend
		else
			depends="$depends, $currdepend"
		fi
	done

cat > tmp/var/lib/mmpack/metadata/$pkgname.pkginfo <<EOF
name: $pkgname
version: $version
description: '$pkgname package description'
source: $pkgname
srcsha256: $(sha256 pkgs/${pkgname}_${version}_src.tar.xz)
depends: $depends
sysdepends:
EOF
}

gen-sha256sums()
{
	for f in $(find tmp -type f -follow -print | LC_ALL=C sort) ; do
		echo "${f#"tmp/"}: reg-$(sha256 $f)"
	done > tmp_sha256sums

	mv tmp_sha256sums tmp/var/lib/mmpack/metadata/${pkgname}.sha256sums
}

gen-mmpack-info()
{
	local pkgname="$1"
	local version="$2"
	shift 2
	local depends="$@"

	echo "$pkgname:" > tmp/MMPACK/info
	if [ -z "$depends" ] ; then
		echo "    depends: {}" >> tmp/MMPACK/info
	else
		echo "    depends:" >> tmp/MMPACK/info
		for dep in "$depends" ; do
			depname=$(echo $dep | cut -f1 -d' ')
			depop=$(echo $dep | cut -f2 -d' ')
			depversion=$(echo $dep | cut -f3 -d' ')
			if [ "$depop" = "=" ]; then
				echo "        $depname: ['$depversion', '$depversion']" >> tmp/MMPACK/info
			else
				echo "        $depname: ['$depversion', any]" >> tmp/MMPACK/info
			fi
		done
	fi

cat << EOF >> tmp/MMPACK/info
    description: '$pkgname package description'
    source: $pkgname
    srcsha256: $(sha256 pkgs/${pkgname}_${version}_src.tar.xz)
    sumsha256sums: $(sha256 tmp/var/lib/mmpack/metadata/$pkgname.sha256sums)
    sysdepends: []
    version: '$version'
    licenses: [dummy]
EOF
}

gen-mmpack-metadata()
{
       local pkgname="$1"
       local version="$2"

cat > tmp/MMPACK/metadata <<EOF
metadata-version: 1.0
name: $pkgname
version: $version
source: $pkgname
srcsha256: $(sha256 pkgs/${pkgname}_${version}_src.tar.xz)
sumsha256sums: $(sha256 tmp/var/lib/mmpack/metadata/$pkgname.sha256sums)
pkginfo-path: ./var/lib/mmpack/metadata/$pkgname.pkginfo
sumsha-path: ./var/lib/mmpack/metadata/$pkgname.sha256sums
EOF
}

gen-manifest()
{
	local pkgname="$1"
	local version="$2"

	local manifest=pkgs/${pkgname}_${version}.mmpack-manifest
	local srcpkg=pkgs/${pkgname}_${version}_src.tar.xz
	local binpkg=pkgs/${pkgname}_${version}.mpk

	cat << EOF > $manifest
name: $pkgname
version: $version
source:
    file: $(basename $srcpkg)
    size: $(stat -c %s $srcpkg)
    sha256: $(sha256 $srcpkg)
binpkgs:
    $arch:
        ${pkgname}:
            file: $(basename $binpkg)
            size: $(stat -c %s $binpkg)
            sha256: $(sha256 $binpkg)
EOF

	echo $manifest
}

gen-mmpack-pkg()
{
	local outrepo="$1"
	local pkgname="$2"
	local version="$3"
	shift 3
	local depends="$@"

	pushd $pkg_builddir > /dev/null

	echo -n "Creating dummy package: $pkgname ($version) ... "

	gen-src-archive "$pkgname" "$version"
	gen-mmpack-pkginfo "$pkgname" "$version" "$depends"
	gen-sha256sums
	gen-mmpack-info "$pkgname" "$version" "$depends"
	gen-mmpack-metadata "$pkgname" "$version"

	tar --zstd --sort=name -cf pkgs/${pkgname}_${version}.mpk --directory=tmp .
	echo "OK"

	manifest=$(gen-manifest $pkgname $version)

	popd > /dev/null

	$modifyrepo -p $outrepo -a $arch add $pkg_builddir/$manifest
}


#
# create the following packages:
#
# * hello-data (1.0.0)
#   - hello-data
#
# * hello-data (2.0.0)
#   - hello-data
#
# * hello (1.0.0)
#   (depends on hello-data)
#   - hello-world (prints the content of hello-data)
#
# * call-hello (1.0.0)
#   (depends on hello)
#   - call-hello.sh (calls hello-world.sh from hello)

# flush all pre-existing repositories
rm -rf $REPO_DIR
mkdir -p $REPO_DIR

# PACKAGE: hello-data
prepare

cat << EOF > $pkg_builddir/tmp/bin/dummy-file
hello world
EOF

gen-mmpack-pkg $REPO_DIR/0 "hello-data" "1.0.0" ""

# PACKAGE: hello-data 2.0.0
prepare

cat << EOF > $pkg_builddir/tmp/bin/dummy-file
hello world v2
EOF

gen-mmpack-pkg $REPO_DIR/1 "hello-data" "2.0.0" ""

# PACKAGE: hello
prepare
cat << EOF > $pkg_builddir/tmp/bin/hello-world
#!/bin/sh -e
cat \$(dirname \$0)/dummy-file
EOF
chmod a+x $pkg_builddir/tmp/bin/hello-world

gen-mmpack-pkg $REPO_DIR/0 "hello" "1.0.0" "hello-data >= 1.0.0"

# PACKAGE: call-hello
prepare
cat << EOF > $pkg_builddir/tmp/bin/call-hello.sh
#!/bin/sh -e
\$(dirname \$0)/hello-world
EOF
chmod a+x $pkg_builddir/tmp/bin/call-hello.sh

gen-mmpack-pkg $REPO_DIR/0 call-hello "1.0.0" "hello >= 1.0.0"

# final cleanup
cleanup
