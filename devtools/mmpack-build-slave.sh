#!/bin/sh

set -e

workdir=$1

export XDG_DATA_HOME=$workdir

tmp_prefix=$workdir/tmp-prefix

# For repository, the URL is obtained from the global config of the user
# that execute the script. (likely set in $HOME/.config/mmpack-config.yaml)
mmpack mkprefix --force $tmp_prefix
mmpack-build pkg-create --prefix=$tmp_prefix --build-deps -y \
 --src=$workdir/sources.tar.gz --skip-build-tests
