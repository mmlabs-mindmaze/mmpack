# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import os
import re
from typing import Set, List, Optional

from . common import shell
from . pe_utils import get_dll_from_soname, symbols_set
from . settings import PACMAN_PREFIX
from . syspkg_manager_base import SysPkgManager
from . workspace import Workspace


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


def msys2_find_pypkg(pypkg: str) -> Optional[str]:
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


class PacmanMsys2(SysPkgManager):
    """
    Class to interact with msys2 package database
    """
    def find_sharedlib_sysdep(self, soname: str, symbols: List[str]) -> str:
        return msys2_find_dependency(soname, symbols)

    def find_pypkg_sysdep(self, pypkg: str) -> Optional[str]:
        return msys2_find_pypkg(pypkg)
