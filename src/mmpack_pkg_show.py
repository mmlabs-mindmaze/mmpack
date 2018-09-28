# @mindmaze_header@
'''
TODOC
This is not mmpack-info.py
could be aliased with mmpack-pkg-info.py
'''

import os
import sys
import tarfile
from glob import glob
from common import pushdir, popdir
from workspace import Workspace


def _usage(progname):
    print('usage: {} <pkg name>'.format(progname))


def _is_primary(pkgname: str) -> bool:
    return not (pkgname.endswith('-devel')
                and pkgname.endswith('-pkg')
                and pkgname.endswith('-debug'))


def _get_pkg_fullpath(pkgname: str) -> str:
    if os.path.isfile(pkgname):
        return pkgname
    else:
        pkgdir = Workspace().packages + '/'
        pushdir(pkgdir)
        candidates = glob('*' + pkgname + '*.mpk')
        popdir()
        candidates = [x for x in candidates if _is_primary(x)]
        if not candidates:
            return None

        pkgname = candidates[0]
        if len(candidates) != 1:
            print('[guess] full package name: {0}'.format(pkgname),
                  file=sys.stderr)
        fullpath = pkgdir + pkgname
        if os.path.isfile(fullpath):
            return fullpath
        raise FileNotFoundError(pkgname + ' not found !')


def main():
    'TODOC'
    pkgname = sys.argv[1]
    full_pkg_path = _get_pkg_fullpath(pkgname)
    if not full_pkg_path:
        return 0
    pkg_archive = tarfile.open(full_pkg_path, 'r:xz')

    # package metadatas
    info = pkg_archive.extractfile('./MMPACK/info')
    msg = info.read().decode('utf-8')
    print(msg)

    print('Files:')
    for file in pkg_archive.getnames():
        if 'MMPACK' not in file:
            print('\t' + file)


if __name__ == '__main__':
    main()
