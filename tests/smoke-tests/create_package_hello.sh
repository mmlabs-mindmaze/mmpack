#!/bin/bash

set -ex

# clean the files and repositories necessary for the tests
cleanup()
{
	rm -rf $TRASH
	rm -rf $CREATE
}
trap cleanup EXIT

extract_sum_sha()
{
	sha256sum $1 | cut -d$' ' -f1
}

cleanup

mkdir -p $CREATE/bin

# creation of a binary that can be executed
{
echo "#!/bin/bash"
echo "exit"
} >> $CREATE/bin/dum.sh

chmod a+x $CREATE/bin/dum.sh

# creation of a binary that do not have permission to be executed
{
echo "#!/bin/bash"
echo "exit"
} >> $CREATE/bin/dummy.sh

# creation of the tar.gz archive of the sources
mkdir $TRASH
tar -cvzf $TRASH/hello.tar.gz --directory=$CREATE/bin .

# creation of the files to inform the sha of the files included in the archive
mkdir -p $CREATE/var/lib/mmpack/metadata

{
echo "bin/dum.sh: reg-$(extract_sum_sha $CREATE/bin/dum.sh)"
echo "bin/dummy.sh: reg-$(extract_sum_sha $CREATE/bin/dummy.sh)"
} >> $CREATE/var/lib/mmpack/metadata/hello.sha256sums

mkdir $CREATE/MMPACK

cat << EOF > } >> $CREATE/MMPACK/info
hello:
    depends: {}
    description: 'hello'
    source: hello
    srcsha256: $(extract_sum_sha $TRASH/hello.tar.gz)
    sumsha256sums: $(extract_sum_sha $CREATE/var/lib/mmpack/metadata/hello.sha256sums)
    sysdepends: []
    version: 1.0.0
EOF

# creation of the mpk archive
tar -cvzf $PACKAGE/hello.mpk --directory=$CREATE .
