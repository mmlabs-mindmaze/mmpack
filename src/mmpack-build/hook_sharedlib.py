# @mindmaze_header@
"""
plugin tracking the exported symbol and dependencies of shared libraries
"""

import importlib
from os.path import dirname
from typing import Set, Dict

from base_hook import BaseHook
from common import shlib_keyname
from file_utils import is_dynamic_library, get_exec_fileformat
from mm_version import Version


class MMPackBuildHook(BaseHook):
    # pylint: disable=too-few-public-methods
    """
    Hook tracking symbol exposed in installed shared library and keep track
    of the library library used in binaries.
    """

    def __init__(self, srcname: str, version: Version, host_archdist: str):
        super().__init__(srcname, version, host_archdist)

        # load python module to use for handling the executable file
        # format of the targeted host
        self._execfmt = get_exec_fileformat(host_archdist)
        self._module = importlib.import_module(self._execfmt + '_utils')

    def get_dispatch(self, install_files: Set[str]) -> Dict[str, Set[str]]:
        pkgs = dict()
        for file in install_files:
            if is_dynamic_library(file, self._arch):
                soname = self._module.soname(file)
                binpkgname = shlib_keyname(soname)

                # add the soname file to the same package
                so_filename = dirname(file) + '/' + soname

                pkgs[binpkgname] = {file, so_filename}

        return pkgs
