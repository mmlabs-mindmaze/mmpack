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

upload() {
	scp ./venv/bin/mmpack-createrepo ~/.local/share/mmpack-packages/*.mpk root@${REPOSITORY}:/var/www/html/$DIST
	ssh root@${REPOSITORY} /var/www/html/mmpack-createrepo /var/www/html/$DIST /var/www/html/$DIST
	mmpack update
}

createpkg() {
	mmpack-build pkg-create --skip-build-tests --url $1 --tag ${2:-mmpack}
}

cd $(dirname $0)/../build

REPOSITORY="${1:-mindmaze-srv-fr-01}"
DIST=$(distrib)

testdir=$(pwd)/venv
python_minor=$(python3 -c 'import sys; print(sys.version_info.minor)')
python_testdir="$testdir/lib/python3.${python_minor}"

tmp_prefix=$(mktemp -d)
export MMPACK_PREFIX="$tmp_prefix"

export PATH="${testdir}/bin:${MMPACK_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LD_LIBRARY_PATH}"
export LIBRARY_PATH="${MMPACK_PREFIX}/lib:${LIBRARY_PATH}"
export CPATH="${MMPACK_PREFIX}/include:${CPATH}"

export VIRTUAL_ENV=${testdir}
export PYTHONPATH="${python_testdir}:$python_testdir}/site-packages:${PYTHONPATH}"

# *start* by copying ssh id
ssh-copy-id root@${REPOSITORY}

# install them within a prefix
mmpack mkprefix --url="http://$REPOSITORY/$DIST" $MMPACK_PREFIX
mmpack update

# smoke test first
createpkg ssh://git@intranet.mindmaze.ch:7999/~ganne/mmpack-hello-world.git master
upload
mmpack install mmpack-hello-world
hello-world
head-libexec-world || echo "failed as expected"
mmpack runprefix head-libexec-world  # this one should work

createpkg "ssh://git@intranet.mindmaze.ch:7999/ed/rtfilter.git"
createpkg "ssh://git@intranet.mindmaze.ch:7999/ed/xdffileio.git"
createpkg "ssh://git@intranet.mindmaze.ch:7999/~ganne/mmlib.git"

upload
mmpack install rtfilter-devel xdffileio-devel
mmpack install librtfilter1  # win32 workaround

createpkg "ssh://git@intranet.mindmaze.ch:7999/ed/mcpanel.git"
createpkg "ssh://git@intranet.mindmaze.ch:7999/ed/eegdev.git"

upload

mmpack install mcpanel-devel eegdev-devel
mmpack install libmcpanel0 libeegdev0 libxdffileio0  # win32 workaround

# create eegview package which depends on mcpanel and eegdev
createpkg "ssh://git@intranet.mindmaze.ch:7999/ed/eegview.git"
upload
