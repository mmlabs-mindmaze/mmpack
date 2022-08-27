# @mindmaze_header@
"""
plugin tracking the exported symbol and dependencies of shared libraries
"""

import importlib
import os
from typing import Set, Dict, List, Optional

from .base_hook import BaseHook
from .common import (list_files, run_cmd, sha256sum, shlib_keyname, wprint,
                     Assert)
from .file_utils import is_dynamic_library, get_exec_fileformat, \
    filetype, is_importlib, get_linked_dll
from .package_info import PackageInfo, DispatchData
from .provide import ProvideList, load_mmpack_provides, pkgs_provides
from .syspkg_manager import get_syspkg_mgr


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


def _extract_debugsym(filename: str, build_id: Optional[str]):
    if build_id is None:
        execdir = os.path.dirname(filename)
        sha = sha256sum(filename)
        dbgfile = f'{execdir}/.debug/{sha}.debug'
    else:
        dbgfile = f'lib/debug/.build-id/{build_id[:2]}/{build_id[2:]}.debug'

    os.makedirs(os.path.dirname(dbgfile), exist_ok=True)
    run_cmd(['objcopy', '--only-keep-debug', filename, dbgfile])
    run_cmd(['objcopy', '--strip-unneeded']
            + ([f'--add-gnu-debuglink={dbgfile}'] if build_id is None else [])
            + [filename])


class MMPackBuildHook(BaseHook):
    """
    Hook tracking symbol exposed in installed shared library and keep track
    of the library library used in binaries.
    """

    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._mmpack_shlib_provides = None

        # load python module to use for handling the executable file
        # format of the targeted host
        self._execfmt = get_exec_fileformat(self._arch)
        modname = f'mmpack_build.{self._execfmt}_utils'
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
        # Find dependencies among cobuilded binary packages
        others_provides = pkgs_provides(others_pkgs, 'sharedlib')
        others_provides.resolve_deps(currpkg, sonames, symbol_set, True)

        # Find dependencies in installed mmpack packages
        self._get_mmpack_provides().resolve_deps(currpkg, sonames, symbol_set)

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
        for filename in list_files('.', exclude_dirs=True):
            file_type = filetype(filename)
            if file_type == 'elf':
                self._module.adjust_runpath(filename)

            if file_type == self._execfmt:
                build_id = self._module.build_id(filename)
                _extract_debugsym(filename, build_id)

    def dispatch(self, data: DispatchData):
        for file in data.unassigned_files.copy():
            if is_dynamic_library(file, self._arch):
                soname = self._module.soname(file)
                binpkgname = shlib_keyname(soname)

                # add the soname file to the same package
                so_filename = os.path.dirname(file) + '/' + soname
                libfiles = {file, so_filename}

                pkg = data.assign_to_pkg(binpkgname, libfiles)
                if not pkg.description:
                    pkg.description = self._src_description + '\n'
                    pkg.description += 'automatically generated around SONAME '
                    pkg.description += soname

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
        shlib_provides.update_from_specs(specs_provides, pkg)

        pkg.provides['sharedlib'] = shlib_provides

    def store_provides(self, pkg: PackageInfo, folder: str):
        filename = f'{folder}/{pkg.name}.symbols.gz'
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
            if file_type == self._execfmt and not inst_file.endswith('.debug'):
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
            errmsg = f'Failed to process all sharedlib symbols of {pkg.name}\n'
            errmsg += 'Remaining symbols:\n\t'
            errmsg += '\n\t'.join(symbols)
            wprint(errmsg)
