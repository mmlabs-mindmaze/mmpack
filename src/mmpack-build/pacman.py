# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import importlib
import os
import re
from typing import Set

from . common import shell
from . settings import PACMAN_PREFIX
from . workspace import Workspace


def pacman_find_dependency(soname: str, symbol_set: Set[str]) -> str:
    """
    find pacman package providing given file

    Raises:
        ShellException: the package could not be found
    """
    pe_utils = importlib.import_module('pe_utils')
    filename = pe_utils.get_dll_from_soname(soname)

    pacman_line = shell('pacman -Qo ' + filename)
    pacman_line = pacman_line.split('is owned by ')[-1]
    # It appears there can only be one package version and we cannot explicit
    # a package version on install using the pacman command
    # ... rolling-release paragigm and all ...
    package, _version = pacman_line.split(' ')

    # prune symbols
    symbol_set.difference_update(pe_utils.symbols_set(filename))

    return package


def pacman_find_pypkg(pypkg: str) -> str:
    """
    Get packages providing files matchine the specified glob pattern
    """
    pattern = r'mingw64/lib/python\d.\d+/site-packages/{}(.py|.*/)' \
              .format(pypkg)
    file_regex = re.compile(pattern)

    pacman_pkgdir = os.path.join(Workspace().cygroot(), PACMAN_PREFIX, 'local')
    for mingw_pkg in os.listdir(pacman_pkgdir):
        instfiles_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'files')
        for line in open(instfiles_filename, 'rt').readlines():
            if file_regex.match(line):
                desc_filename = os.path.join(pacman_pkgdir, mingw_pkg, 'desc')
                return open(desc_filename, 'rt').readlines(2)

    return None
