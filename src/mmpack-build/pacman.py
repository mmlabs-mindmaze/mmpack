# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import os
import re
import tarfile
from copy import copy
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
    def __init__(self):
        self.name = None
        self.version = None
        self.source = None
        self.filename = None
        self.url = None

    def parse_metadata_from_file(self, desc: str):
        key = ''
        for line in desc.split('\n'):
            if line == '%NAME%':
                key = 'name'
            elif line == '%VERSION%':
                key = 'version'
            elif line == '%BASE%':
                key = 'source'
            elif key == '%FILENAME%':
                key = 'filename'
            elif line:
                # Set value
                self.__setattr__(key, line)

    def fetch_and_extract(self, builddir: str, unpackdir: str) -> List[str]:
        # download package
        pkg_file = os.path.join(builddir, os.path.basename(self.filename))
        download(self.url, pkg_file)

        # Unpack it and add extracted files to package mapping for this syspkg
        with tarfile.open(pkg_file) as pkgtar:
            pkgtar.extractall(unpackdir)
            files = pkgtar.getnames()

        # Strip './' prefix and discard folder
        files = [f.lstrip('./') for f in files if not os.path.isdir(f)]

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
    with tarfile.open(pkgindex, 'r:xz') as tar:
        for fileinfo in tar.getmembers():
            if os.path.basename(fileinfo.name) == 'desc':
                pkg.parse_metadata_from_file(fileinfo.tobuf())
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
    return _parse_pkgindex_comp(repo_url, 'mingw', builddir, source)


def pacman_fetch_unpack(srcpkg: str, builddir: str,
                        unpackdir: str) -> Dict[str, str]:
    repo_url = 'http://repo.msys2.org'
    pkg_list = _parse_pkgindex(repo_url, builddir, srcpkg)

    if not pkg_list:
        raise Assert('No system package matching project {} found'
                     .format(srcpkg))

    files_sysdep = dict()
    for pkg in pkg_list:
        files = pkg.fetch_and_extract(builddir, unpackdir)
        files_sysdep.update(dict.fromkeys(files, pkg.package))

    version = pkg_list[0].version
    return (version, files_sysdep)
