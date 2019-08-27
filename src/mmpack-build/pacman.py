# @mindmaze_header@
"""
helper module containing pacman wrappers and file parsing functions
"""

import importlib
from typing import Set

from common import shell


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
    pe_utils.prune_symbols(filename, symbol_set)

    return package
