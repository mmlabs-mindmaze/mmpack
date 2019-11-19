# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import os
import re
import tarfile
from copy import copy
from io import TextIOBase, TextIOWrapper
from typing import Set, Dict, List

from . common import *
from . pe_utils import get_dll_from_soname, symbols_set
from . settings import PACMAN_PREFIX
from . workspace import Workspace


def pacman_find_dependency(soname: str, symbol_set: Set[str]) -> str:
    """
    find pacman package providing given file

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


def pacman_find_pypkg(pypkg: str) -> str:
    """
    Get packages providing files matchine the specified glob pattern
    """

    # either match:
    # pypkg = 'numpy'
    # mingw64/lib/python3.7/site-packages/numpy/
    # mingw64/lib/python3.7/site-packages/numpy.py
    pattern = r'mingw64/lib/python\d.\d+/site-packages/{}(.py|/.*)' \
              .format(pypkg)
    file_regex = re.compile(pattern)

    pacman_pkgdir = Workspace().cygroot() + PACMAN_PREFIX + '\\local'
    for mingw_pkg in os.listdir(pacman_pkgdir):
        instfiles_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'files')
        if not os.path.isfile(instfiles_filename):
            continue

        for line in open(instfiles_filename, 'rt').readlines():
            if file_regex.match(line):
                desc_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'desc')
                return open(desc_filename, 'rt').readlines()[1].strip()

    return None


class PacmanPkg:
    """
    representation of a package in a distribution repository using pacman
    """
    desc_field_re = re.compile(r'%([A-Z]+)%')
    desc_field_to_attr = {
        'NAME': 'name',
        'VERSION': 'version',
        'BASE': 'source',
        'FILENAME': 'filename',
    }

    def __init__(self):
        self.name = None
        self.version = None
        self.source = None
        self.filename = None
        self.url = None

    def parse_metadata_from_file(self, fileobj: TextIOBase):
        """
        Parse package description
        """
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
                self.__setattr__(self.desc_field_to_attr[key], value)

    def fetch_and_extract(self, builddir: str, unpackdir: str) -> List[str]:
        """
        Download and extract described system package

        Args:
            builddir: folder where to put downloaded packages
            unpackdir: location where to unpack the downloaded system package

        Return: list of files unpacked from the system package (dirs excluded)
        """
        # download package
        pkg_file = os.path.join(builddir, os.path.basename(self.filename))
        download(self.url, pkg_file)

        # Unpack it and add extracted files to package mapping for this syspkg
        ignored = ['.BUILDINFO', '.MTREE', '.PKGINFO']
        with tarfile.open(pkg_file) as pkgtar:
            members = [i for i in pkgtar.getmembers() if i.name not in ignored]
            pkgtar.extractall(unpackdir, members)
            files = [i.name for i in members]

        return files


def _get_msys2_repo_comp(component, arch):
    if component != 'mingw':
        return component

    if arch == 'amd64':
        return 'mingw64'
    elif arch == 'i386':
        return 'mingw32'
    else:
        raise Assert('unexpected architecture: {}'.format(arch))


def _get_repo_cpu(arch):
    if arch == 'amd64':
        return 'x86_64'
    elif arch == 'i386':
        return 'i686'
    else:
        raise Assert('unexpected architecture: {}'.format(arch))


def _parse_pkgindex_comp(repo_url, component: str, builddir: str,
                         source: str) -> List[PacmanPkg]:
    pkg_list = []
    arch = get_host_arch()

    cpu_arch = _get_repo_cpu(arch)
    repo = _get_msys2_repo_comp(component, arch)
    pkgindex_url = '{}/{}/{}/{}.db'.format(repo_url, component, cpu_arch, repo)
    pkgindex = os.path.join(builddir, os.path.basename(pkgindex_url))
    pkg_baseurl = '{}/{}/{}/'.format(repo_url, component, cpu_arch)

    # download compressed index
    download(pkgindex_url, pkgindex)

    # Parse actual index
    pkg = PacmanPkg()
    with tarfile.open(pkgindex, 'r') as tar:
        for fileinfo in tar.getmembers():
            if os.path.basename(fileinfo.name) == 'desc':
                with TextIOWrapper(tar.extractfile(fileinfo)) as desc_fileobj:
                    pkg.parse_metadata_from_file(desc_fileobj)
                if pkg.source == source:
                    pkg.url = pkg_baseurl + pkg.filename
                    pkg_list.append(copy(pkg))

    return pkg_list


def _parse_pkgindex(repo_url: str, builddir: str,
                    source: str) -> List[PacmanPkg]:
    # Search first in mingw* repository
    mingw_source = 'mingw-w64-' + source
    pkg_list = _parse_pkgindex_comp(repo_url, 'mingw', builddir, mingw_source)
    if pkg_list:
        return pkg_list

    # Fallback to msys repository
    return _parse_pkgindex_comp(repo_url, 'msys', builddir, source)


def pacman_fetch_unpack(srcpkg: str, builddir: str,
                        unpackdir: str) -> (str, Dict[str, str]):
    """
    Find which package in given list provides some file.

    Return:
        Couple of system package version as downloaded and the mapping of file
        to package name that provides them
    """
    repo_url = 'http://repo.msys2.org'
    pkg_list = _parse_pkgindex(repo_url, builddir, srcpkg)

    if not pkg_list:
        raise Assert('No system package matching project {} found'
                     .format(srcpkg))

    files_sysdep = dict()
    for pkg in pkg_list:
        files = pkg.fetch_and_extract(builddir, unpackdir)
        files_sysdep.update(dict.fromkeys(files, pkg.name))

    version = pkg_list[0].version
    return (version, files_sysdep)
