#!/bin/sh
#
# quick helper to create some pacakges
# this is intended do be called manually

set -ex

distrib() {
	if [ "$(cat /etc/os-release  | grep "^ID=")" == "ID=debian" ]; then
		echo debian
	else
		echo windows
	fi
}

cd $(dirname $0)/../build

REPOSITORY="${1:-mindmaze-srv-fr-01}"
DIST=$(distrib)

testdir=$(pwd)/venv
python_minor=$(python3 -c 'import sys; print(sys.version_info.minor)')
python_testdir="$testdir/lib/python3.${python_minor}"

export VIRTUAL_ENV=$testdir
export PYTHONPATH="$python_testdir:$python_testdir/site-packages:$PYTHONPATH"
export PATH="$testdir/bin:$PATH"
export LD_LIBRARY_PATH="$testdir/lib:$LD_LIBRARY_PATH"

URL_LIST='
ssh://git@intranet.mindmaze.ch:7999/~ganne/mmlib.git
ssh://git@intranet.mindmaze.ch:7999/ed/mcpanel.git
ssh://git@intranet.mindmaze.ch:7999/ed/eegdev.git
'

# build packages
for url in $URL_LIST
do
	mmpack-build pkg-create --skip-build-tests --url $url --tag mmpack
done

# upload packages
scp ./venv/bin/mmpack-createrepo ~/.local/share/mmpack-packages/*.mpk root@${REPOSITORY}:/var/www/html/$DIST
ssh root@${REPOSITORY} /var/www/html/mmpack-createrepo /var/www/html/$DIST /var/www/html/$DIST

# install them within a prefix

tmp_prefix=$(mktemp -d)
export MMPACK_PREFIX="$tmp_prefix"
./mmpack mkprefix --url="http://$REPOSITORY/$DIST" $MMPACK_PREFIX
./mmpack update
./mmpack install mcpanel-devel eegdev-devel

# create eegview package which depends on mcpanel and eegdev
url='ssh://git@intranet.mindmaze.ch:7999/ed/eegview.git'
mmpack-build pkg-create --skip-build-tests --url $url --tag mmpack
