# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import os
import re
import tarfile

from contextlib import contextmanager
from io import BytesIO, TextIOWrapper
from typing import Set, List, TextIO

from .common import *
from .errors import ShellException
from .pe_utils import get_dll_from_soname, symbols_set
from .settings import PACMAN_PREFIX
from .syspkg_manager_base import SysPkgManager, SysPkg
from .workspace import Workspace, cached_download


def msys2_find_dependency(soname: str, symbol_set: Set[str]) -> str:
    """
    find msys2 package providing given file

    Raises:
        ShellException: the package could not be found
    """
    filename = get_dll_from_soname(soname)

    pacman_line = shell('pacman -Qo ' + filename)
    pacman_line = pacman_line.split('is owned by ')[-1]
    # It appears there can only be one package version and we cannot explicit
    # a package version on install using the pacman command
    # ... rolling-release paragigm and all ...
    package, _version = pacman_line.split(' ')

    # prune symbols
    symbol_set.difference_update(symbols_set(filename))

    return package


def msys2_find_pypkg(pypkg: str) -> str:
    """
    Get packages providing files matchine the specified glob pattern
    """

    # either match:
    # pypkg = 'numpy'
    # mingw64/lib/python3.7/site-packages/numpy/
    # mingw64/lib/python3.7/site-packages/numpy.py
    pattern = rf'mingw64/lib/python\d.\d+/site-packages/{pypkg}(.py|/.*)'
    file_regex = re.compile(pattern)

    pacman_pkgdir = Workspace().cygroot() + PACMAN_PREFIX + '\\local'
    for mingw_pkg in os.listdir(pacman_pkgdir):
        instfiles_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'files')
        if not os.path.isfile(instfiles_filename):
            continue

        # Return 2nd line of desc file if any installed file match pattern
        with open(instfiles_filename, 'rt', encoding='utf-8') as instfile:
            if any(True for l in instfile if file_regex.match(l)):
                desc_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'desc')
                with open(desc_filename, 'rt', encoding='utf-8') as stream:
                    # Return second line of file
                    _ = next(stream)
                    return next(stream).strip()

    return None


class PacmanPkg(SysPkg):
    """
    representation of a package in a distribution repository using pacman
    """
    desc_field_re = re.compile(r'%([A-Z]+)%')
    desc_field_to_attr = {
        'NAME': 'name',
        'VERSION': 'version',
        'BASE': 'source',
        'FILENAME': 'filename',
        'SHA256SUM': 'sha256',
    }

    def __init__(self, fileobj: TextIO):
        """
        Parse package description

        Args:
            fileobj: stream reading downloaded repository index

        Return: 1 if a package has been parsed, 0 if end of file is reached
        """
        super().__init__()
        key = None
        for line in fileobj:
            value = line.strip()
            match = self.desc_field_re.fullmatch(value)
            if match:
                key = match.group(1)
                value = None  # new key, so reset value
                if key not in self.desc_field_to_attr:
                    key = None
            elif key and value:
                setattr(self, self.desc_field_to_attr[key], value)


def _get_msys2_repo_comp(component, arch):
    if component != 'mingw':
        return component

    mingw_subcomps = {'amd64': 'mingw64', 'i386': 'mingw32'}
    val = mingw_subcomps.get(arch)
    if not val:
        raise Assert(f'unexpected architecture: {arch}')
    return val


def _get_repo_cpu(arch):
    repo_cpu_map = {'amd64': 'x86_64', 'i386': 'i686'}
    val = repo_cpu_map.get(arch)
    if not val:
        raise Assert(f'unexpected architecture: {arch}')
    return val


@contextmanager
def _open_db_tarball(filename: str):
    # Read whole uncompressed tar in memory because tar decompression will
    # require later random access
    with open_compressed_file(filename, 'rb') as zstdfile:
        buff_io = BytesIO(zstdfile.read())

    try:
        tar = tarfile.open(fileobj=buff_io, mode='r:')
        yield tar
        tar.close()
    finally:
        buff_io.close()


def _parse_pkgindex_comp(repo_url, component: str, builddir: str,
                         source: str) -> List[PacmanPkg]:
    pkg_list = []
    arch = get_host_arch()

    cpu_arch = _get_repo_cpu(arch)
    repo = _get_msys2_repo_comp(component, arch)
    pkgindex_url = f'{repo_url}/{component}/{cpu_arch}/{repo}.db'
    pkgindex = os.path.join(builddir, os.path.basename(pkgindex_url))
    pkg_baseurl = f'{repo_url}/{component}/{cpu_arch}/'

    # download compressed index
    cached_download(pkgindex_url, pkgindex)

    # Parse actual index
    with _open_db_tarball(pkgindex) as tar:
        for fileinfo in tar.getmembers():
            if os.path.basename(fileinfo.name) == 'desc':
                pkg = PacmanPkg(TextIOWrapper(tar.extractfile(fileinfo),
                                              encoding='utf-8'))
                if pkg.source == source:
                    pkg.url = pkg_baseurl + pkg.filename
                    pkg_list.append(pkg)

    return pkg_list


def _get_repo_baseurl() -> str:
    return os.getenv('MMPACK_BUILD_MSYS2_REPO', 'http://repo.msys2.org')


class PacmanMsys2(SysPkgManager):
    """
    Class to interact with msys2 package database
    """
    def find_sharedlib_sysdep(self, soname: str, symbols: List[str]) -> str:
        return msys2_find_dependency(soname, symbols)

    def find_pypkg_sysdep(self, pypkg: str) -> str:
        return msys2_find_pypkg(pypkg)

    def _extract_syspkg(self, pkgfile: str, unpackdir: str) -> List[str]:
        # Unpack and add extracted files to package mapping for this syspkg
        # Python tarfile module is not used because it currently does not
        # support z-standart compression (.tar.zst)
        pkgfile_u = shell(['cygpath', '-u', pkgfile], log=False).strip()
        unpackdir_u = shell(['cygpath', '-u', unpackdir], log=False).strip()
        cmd = f'tar -xvf {pkgfile_u} -C {unpackdir_u} --exclude=.BUILDINFO '\
              '--exclude=.MTREE --exclude=.PKGINFO'
        files = shell(cmd).splitlines()
        return [f.rstrip('/') for f in files]

    def parse_pkgindex(self, builddir: str,
                       srcnames: List[str]) -> List[SysPkg]:
        repo_url = _get_repo_baseurl()

        for srcname in srcnames:
            # Search first in mingw* repository
            pkgs = _parse_pkgindex_comp(repo_url, 'mingw', builddir,
                                        'mingw-w64-' + srcname)
            if pkgs:
                return pkgs

            # Fallback to msys repository
            pkgs = _parse_pkgindex_comp(repo_url, 'msys', builddir, srcname)
            if pkgs:
                return pkgs

        return []
