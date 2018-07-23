# @mindmaze_header@
'''
TODOC
'''

import os
from os.path import isfile
from glob import glob
import tarfile
from common import sha256sum, yaml_serialize, pushdir, popdir
from version import Version


def _reset_entry_attrs(tarinfo: tarfile.TarInfo):
    '''
    filter function for tar creation that will remove all file attributes
    (uid, gid, mtime) from the file added to tar would can make the build
    of package not reproducible.

    Args:
        tarinfo: entry being added to the tar
    Return:
        the modified tarinfo that will be actually added to tar
    '''
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = 'root'
    tarinfo.mtime = 0
    return tarinfo


class BinaryPackage(object):
    # pylint: disable=too-many-instance-attributes
    '''
    TODOC
    '''
    def __init__(self, name: str, version: Version, source: str, arch: str):
        '''
        TODOC
        '''
        self.name = name
        self.version = version
        self.source = source
        self.arch = arch

        self.description = ''
        self._dependencies = {'sysdepends': {}, 'depends': {}}
        self._symbols = {}
        self._install_files = []

    def _gen_info(self, pkgdir: str) -> None:
        pushdir(pkgdir)

        # Create file containing of hashes of all installed files
        cksums = {}
        for filename in glob('**', recursive=True):
            # skip folder and MMPACK/info
            if not isfile(filename) or filename == 'MMPACK/info':
                continue

            # Add file with checksum
            cksums[filename] = sha256sum(filename)
        yaml_serialize(cksums, 'MMPACK/sha256sums')

        # Create info file
        info = {'version': self.version,
                'source': self.source,
                'description': self.description,
                'sumsha256sums': sha256sum('MMPACK/sha256sums')}
        info.update(self._dependencies)
        yaml_serialize({self.name: info}, 'MMPACK/info')

        popdir()

    def _populate(self, instdir: str, pkgdir: str) -> None:
        for instfile in self._install_files:
            src = instdir + instfile
            dst = pkgdir + instfile
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.link(src, dst, follow_symlinks=False)

    def _make_archive(self, pkgdir: str, dstdir: str) -> str:
        mpkfile = "{0}/{1}_{2}_{3}.mpk".format(dstdir, self.name,
                                               self.version, self.arch)
        tar = tarfile.open(mpkfile, 'x:xz')
        tar.add(pkgdir, recursive=True, filter=_reset_entry_attrs, arcname='.')
        tar.close()

        return mpkfile

    def create(self, dstdir: str, builddir: str) -> str:
        '''
        TODOC
        '''
        pkgdir = builddir + '/' + self.name + '/'
        instdir = builddir + '/local-install/'
        os.makedirs(pkgdir + 'MMPACK', exist_ok=True)

        self._populate(instdir, pkgdir)
        self._gen_info(pkgdir)
        return self._make_archive(pkgdir, dstdir)

    def add_depend(self, name: str, version: Version) -> None:
        '''
        Add mmpack package as a dependency with a minimal version
        '''
        dependencies = self._dependencies['depends']
        curr_version = dependencies.get(name)
        if not curr_version or curr_version < version:
            dependencies[name] = version

    def add_sysdepend(self, name: str) -> None:
        '''
        Add a system dependencies to the binary package
        '''
        self._dependencies['sysdepends'][name] = 'any'
