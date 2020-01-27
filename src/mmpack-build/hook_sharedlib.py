# @mindmaze_header@
"""
plugin tracking the exported symbol and dependencies of shared libraries
"""

import importlib
import os
from glob import glob
from typing import Set, Dict, List

from . base_hook import BaseHook, PackageInfo
from . common import shlib_keyname, Assert
from . file_utils import is_dynamic_library, get_exec_fileformat, \
    filetype, is_importlib, get_linked_dll
from . provide import ProvideList, load_mmpack_provides
from . syspkg_manager import get_syspkg_mgr


def _add_dll_dep_to_pkginfo(currpkg: PackageInfo, import_lib: str,
                            other_pkgs: List[PackageInfo]) -> None:
    """
    Adds to dependencies the package that hosts the dll associated with
    import library.
    """
    dll = get_linked_dll(import_lib)
    for pkg in other_pkgs:
        pkg_dlls = [os.path.basename(f).lower() for f in pkg.files
                    if f.endswith(('.dll', '.DLL'))]
        if dll in pkg_dlls:
            if pkg.name != currpkg.name:
                currpkg.add_to_deplist(pkg.name, pkg.version, pkg.version)
            return


class MMPackBuildHook(BaseHook):
    """
    Hook tracking symbol exposed in installed shared library and keep track
    of the library library used in binaries.
    """

    def __init__(self, srcname: str, host_archdist: str):
        super().__init__(srcname, host_archdist)
        self._mmpack_shlib_provides = None

        # load python module to use for handling the executable file
        # format of the targeted host
        self._execfmt = get_exec_fileformat(host_archdist)
        modname = 'mmpack_build.{}_utils'.format(self._execfmt)
        self._module = importlib.import_module(modname)

    def _get_mmpack_provides(self) -> ProvideList:
        """
        Get all shared library soname and associated symbols for all mmpack
        package installed in prefix. The parsing of all .symbols files is
        cached, hence subsequent calls to this method is very fast.
        """
        if not self._mmpack_shlib_provides:
            self._mmpack_shlib_provides = load_mmpack_provides('symbols',
                                                               'sharedlib')
        return self._mmpack_shlib_provides

    def _gen_shlib_deps(self, currpkg: PackageInfo,
                        sonames: Set[str], symbol_set: Set[str],
                        others_pkgs: List[PackageInfo]):
        """
        For each element in `sonames` determine the mmpack or system
        dependency that provides it and adds it to those of `currpkg`.
        When an soname is found, its associated symbol_set are discarded from
        `symbol_set`.

        Args:
            currpkg: package whose dependencies are computed (and added)
            sonames: set of sonames used in currpkg
            symbol_set: set of symbols used in currpkg
            others_pkgs: list of packages cobuilded (may include currpkg)

        Raises:
            Assert: a used soname is not provided by any mmpack or
                system package.
        """
        # provided in the same package or a package being generated
        for pkg in others_pkgs:
            dep_list = pkg.provides['sharedlib'].gen_deps(sonames, symbol_set)
            for pkgname, _ in dep_list:
                currpkg.add_to_deplist(pkgname, pkg.version, pkg.version)

        # provided by another mmpack package present in the prefix
        dep_list = self._get_mmpack_provides().gen_deps(sonames, symbol_set)
        for pkgname, version in dep_list:
            currpkg.add_to_deplist(pkgname, version)

        # provided by the host system
        syspkg_mgr = get_syspkg_mgr()
        for soname in sonames:
            sysdep = syspkg_mgr.find_sharedlib_sysdep(soname, symbol_set)
            if not sysdep:
                # <soname> dependency could not be met with any available means
                errmsg = 'Could not find package providing ' + soname
                raise Assert(errmsg)

            currpkg.add_sysdep(sysdep)

    def post_local_install(self):
        """
        For ELF binaries (executable and shared lib), ensure that DT_RUNPATH is
        set at least to value suitable for mmpack, modify it if necessary.
        """
        # This step is only relevant for ELF
        if self._execfmt != 'elf':
            return

        for filename in glob('**', recursive=True):
            if filetype(filename) == 'elf':
                self._module.adjust_runpath(filename)

    def dispatch(self, install_files: Set[str], pkgs: Dict[str, PackageInfo]):
        for file in install_files:
            if is_dynamic_library(file, self._arch):
                soname = self._module.soname(file)
                binpkgname = shlib_keyname(soname)

                # add the soname file to the same package
                so_filename = os.path.dirname(file) + '/' + soname

                pkg = PackageInfo(binpkgname)
                pkg.files.update({file, so_filename})
                pkgs[binpkgname] = pkg

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
            provide = self._module.ShlibProvide(name, soname)
            provide.pkgdepends = pkg.name
            provide.add_symbols(symbols, pkg.version)
            shlib_provides.add(provide)

        # update symbol information from .provides file if any
        shlib_provides.update_from_specs(specs_provides, pkg.name)

        pkg.provides['sharedlib'] = shlib_provides

    def store_provides(self, pkg: PackageInfo, folder: str):
        filename = '{}/{}.symbols'.format(folder, pkg.name)
        pkg.provides['sharedlib'].serialize(filename)

    def update_depends(self, pkg: PackageInfo, other_pkgs: List[PackageInfo]):
        deps = set()
        symbols = set()
        for inst_file in pkg.files:
            if is_importlib(inst_file):
                _add_dll_dep_to_pkginfo(pkg, inst_file, other_pkgs)
                continue

            # Ignore dependency finding if ghost package (link of importlib to
            # dll should have been however be performed)
            if pkg.ghost:
                continue

            # populate the set of sonames of shared libraries used by the
            # file and the set of used symbols external to the file. This
            # will be use to determine the dependencies
            file_type = filetype(inst_file)
            if file_type == self._execfmt:
                symbols.update(self._module.undefined_symbols(inst_file))
                deps.update(self._module.soname_deps(inst_file))

        # Given the set of sonames and used symbols by all file in the
        # package, determine the actual package dependencies, ie find which
        # package provides the used shared libraries and symbols. The
        # symbols set helps to determine the minimal version to use for
        # each dependency found. Once a shared lib package dependency is
        # found, associated symbols are discarded from symbols set.
        self._gen_shlib_deps(pkg, deps, symbols, other_pkgs)

        if symbols:
            errmsg = 'Failed to process all sharedlib symbols of {}\n' \
                     .format(pkg.name)
            errmsg += 'Remaining symbols:\n\t'
            errmsg += '\n\t'.join(symbols)
            raise Assert(errmsg)
