#!/bin/sh
#
# quick helper to create some pacakges
# this is intended do be called manually

set -ex

distrib() {
	grep -q "^ID=debian" /etc/os-release
	if [ $? -eq 0 ]; then
		echo debian
	else
		echo windows
	fi
}

upload() {
	scp ./venv/bin/mmpack-createrepo ~/.local/share/mmpack-packages/*.mpk root@${REPOSITORY}:/var/www/html/$DIST
	scp ./venv/bin/mmpack-createrepo ~/.local/share/mmpack-packages/*_src.tar.gz root@${REPOSITORY}:/var/www/html/$DIST
	ssh root@${REPOSITORY} /var/www/html/mmpack-createrepo /var/www/html/$DIST /var/www/html/$DIST
	mmpack update
}

createpkg() {
	mmpack-build pkg-create --skip-build-tests --url $1 --tag ${2:-master}
}

if [ ! -d venv ] ; then
	echo "must run from build folder after make check"
	exit 1
fi

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

# Ensure we can ssh without asking password all the time
if ! ssh -o "PasswordAuthentication no" root@${REPOSITORY} true; then
	echo "Cannot ssh as root to ${REPOSITORY} using pubkey"
	exit 1
fi

# install them within a prefix
mmpack mkprefix --url="http://$REPOSITORY/$DIST" $MMPACK_PREFIX
mmpack update

# smoke test first
createpkg ssh://git@intranet.mindmaze.ch:7999/~ganne/mmpack-hello-world.git
upload
mmpack install -y mmpack-hello-world
hello-world
head-libexec-world || echo "failed as expected"
mmpack runprefix head-libexec-world  # this one should work

if [ -n "$SMOKE" ] ; then
	exit
fi

createpkg "ssh://intranet.mindmaze.ch:29418/rtfilter.git"
createpkg "ssh://intranet.mindmaze.ch:29418/xdffileio.git"
createpkg "ssh://intranet.mindmaze.ch:29418/mmlib.git"

upload
mmpack install -y rtfilter-devel xdffileio-devel mmlib-devel
mmpack install -y librtfilter1 libxdffileio0 libmmlib0 # win32 workaround

createpkg "ssh://intranet.mindmaze.ch:29418/mcpanel.git"
createpkg "ssh://intranet.mindmaze.ch:29418/eegdev.git"

upload

mmpack install -y mcpanel-devel eegdev-devel
mmpack install -y libmcpanel0 libeegdev0  # win32 workaround

# create eegview package which depends on mcpanel and eegdev
createpkg "ssh://intranet.mindmaze.ch:29418/eegview.git"
upload
