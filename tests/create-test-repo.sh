#!/bin/bash

set -e

DEPLOYMENT_DIR=$1
REPO_DIR=$2

if [ -n "$(which cygpath)" ] ; then
	DEPLOYMENT_DIR=$(cygpath -u $DEPLOYMENT_DIR)
	REPO_DIR=$(cygpath -u $REPO_DIR)

	# minimal system dependency that we expect to always be here
	sysdep="mingw-w64-x86_64-gcc-libs"
else
	sysdep="libc6 (>= 2.15)"
fi

tmp=$REPO_DIR/tmp

# create another tmp workdir to prevent including self when globbing
tmp2=$REPO_DIR/tmp2

cleanup()
{
	rm -rf $tmp
	rm -rf $tmp2
}
trap cleanup EXIT

prepare()
{
	cleanup

	mkdir $tmp2
	mkdir -p $tmp/bin

	mkdir -p $tmp/MMPACK
	mkdir -p $tmp/var/lib/mmpack/metadata/
}

gen-src-archive()
{
	local pkgname=$1
	local version="$2"

	tar -czf $REPO_DIR/${pkgname}_${version}_src.tar.gz --directory=$tmp .
}

sha256()
{
	sha256sum $1 | cut -d$' ' -f1
}

gen-sha256sums()
{
	for f in $(find $tmp -type f -follow -print) ; do
		echo "${f#"$tmp/"}: reg-$(sha256 $f)"
	done
}

gen-mmpack-info()
{
	local pkgname="$1"
	local version="$2"
	shift 2
	local depends="$@"

	echo "$pkgname:" > $tmp/MMPACK/info
	if [ -z "$depends" ] ; then
		echo "    depends: {}" >> $tmp/MMPACK/info
	else
		echo "    depends:" >> $tmp/MMPACK/info
		for dep in "$depends" ; do
			echo "        $dep" >> $tmp/MMPACK/info
		done
	fi

cat << EOF >> $tmp/MMPACK/info
    description: '$pkgname package description'
    source: $pkgname
    srcsha256: $(sha256 $REPO_DIR/${pkgname}_${version}_src.tar.gz)
    sumsha256sums: $(sha256 $tmp/var/lib/mmpack/metadata/$pkgname.sha256sums)
    sysdepends: [$sysdep]
    version: '$version'
    licenses: [dummy]
EOF
}

gen-mmpack-pkg()
{
	local pkgname="$1"
	local version="$2"
	shift 2
	local depends="$@"

	echo -n "Creating dummy package: $pkgname ($version) ... "

	gen-src-archive "$pkgname" "$version"
	gen-sha256sums > $tmp2/tmp
	mv $tmp2/tmp $tmp/var/lib/mmpack/metadata/${pkgname}.sha256sums
	gen-mmpack-info "$pkgname" "$version" "$depends"

	tar -czf $REPO_DIR/${pkgname}_${version}.mpk --directory=$tmp .
	echo "OK"
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

cat << EOF > $tmp/bin/dummy-file
hello world
EOF

gen-mmpack-pkg "hello-data" "1.0.0" ""

# PACKAGE: hello-data 2.0.0
prepare

cat << EOF > $tmp/bin/dummy-file
hello world v2
EOF

gen-mmpack-pkg "hello-data" "2.0.0" ""

# PACKAGE: hello
prepare
cat << EOF > $tmp/bin/hello-world
#!/bin/sh -e
cat \$(dirname \$0)/dummy-file
EOF
chmod a+x $tmp/bin/hello-world

gen-mmpack-pkg "hello" "1.0.0" "hello-data: ['1.0.0', any]"

# PACKAGE: call-hello
prepare
cat << EOF > $tmp/bin/call-hello.sh
#!/bin/sh -e
\$(dirname \$0)/hello-world
EOF
chmod a+x $tmp/bin/call-hello.sh

gen-mmpack-pkg call-hello "1.0.0" "hello: ['1.0.0', any]"

# create the binary index
# use the create-repo script from the test deployment dir
createrepo=$(find $DEPLOYMENT_DIR -type f -follow -name mmpack-createrepo)

$createrepo $REPO_DIR $REPO_DIR

# final cleanup
cleanup
