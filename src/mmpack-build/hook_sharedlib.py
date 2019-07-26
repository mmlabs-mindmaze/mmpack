# @mindmaze_header@
"""
plugin tracking the exported symbol and dependencies of shared libraries
"""

import importlib
from os.path import dirname
from typing import Set, Dict

from base_hook import BaseHook, PackageInfo
from common import shlib_keyname
from file_utils import is_dynamic_library, get_exec_fileformat
from mm_version import Version
from provide import Provide, ProvideList


class MMPackBuildHook(BaseHook):
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

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        shlib_provides = ProvideList('sharedlib')
        for inst_file in pkg.files:
            if not is_dynamic_library(inst_file, self._arch):
                continue

            # Get SONAME of the library, its exported symbols and compute
            # the package dependency to use from the SONAME
            soname = self._module.soname(inst_file)
            symbols = self._module.symbols_set(inst_file)
            name = shlib_keyname(soname)

            # store information about exported soname, symbols and package
            # to use in the provide list
            provide = Provide(name, soname)
            provide.pkgdepends = pkg.name
            provide.add_symbols(symbols, self._version)
            shlib_provides.add(provide)

        # update symbol information from .provides file if any
        shlib_provides.update_from_specs(specs_provides, pkg.name)

        pkg.provides['sharedlib'] = shlib_provides

    def store_provides(self, pkg: PackageInfo, folder: str):
        filename = '{}/{}.symbols'.format(folder, pkg.name)
        pkg.provides['sharedlib'].serialize(filename)
