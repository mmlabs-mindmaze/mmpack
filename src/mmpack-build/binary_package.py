# @mindmaze_header@
"""
Class to handle binary packages, their dependencies, symbol interface, and
packaging as mmpack file.
"""

import importlib
import os
import tarfile

from glob import glob
from os.path import isfile, basename
from typing import List, Dict, Set

from common import *
from dpkg import dpkg_find_dependency
from file_utils import filetype, is_dynamic_library, \
    is_importlib, get_linked_dll, get_exec_fileformat
from mm_version import Version
from pacman import pacman_find_dependency
from settings import DPKG_PREFIX, PACMAN_PREFIX
from workspace import Workspace, get_staging_dir


def _reset_entry_attrs(tarinfo: tarfile.TarInfo):
    """
    filter function for tar creation that will remove all file attributes
    (uid, gid, mtime) from the file added to tar would can make the build
    of package not reproducible.

    Args:
        tarinfo: entry being added to the tar

    Returns:
        the modified tarinfo that will be actually added to tar
    """
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = 'root'
    tarinfo.mtime = 0

    if tarinfo.name.lower().endswith('.dll'):
        tarinfo.mode = 0o755

    return tarinfo


def _sysdep_find_dependency(soname: str, symbol_set: Set[str]) -> str:
    wrk = Workspace()
    if os.path.exists(DPKG_PREFIX):
        return dpkg_find_dependency(soname, symbol_set)
    if os.path.exists(wrk.cygroot() + PACMAN_PREFIX):
        return pacman_find_dependency(soname, symbol_set)

    raise FileNotFoundError('Could not find system package manager')


def _mmpack_lib_minversion(metadata: Dict[str, Dict[str, Version]],
                           symbols: Set[str]) -> Version:
    min_version = None
    for sym in list(symbols):  # iterate over a shallow copy of the list
        if sym in metadata['symbols']:
            metadata_version = Version(metadata['symbols'][sym])
            if min_version:
                min_version = max(min_version, metadata_version)
            else:
                min_version = metadata_version
            symbols.remove(sym)  # remove symbol from the list

    return min_version


def _mmpack_lib_deps(soname: str,
                     symbols: Set[str]) -> (str, Version, Version):
    wrk = Workspace()
    symbols_path = wrk.prefix + '/var/lib/mmpack/metadata/'

    # Start list by symbol file based on a guess of package name that might
    # provide soname and add all other symbol file that can be found
    libname, soversion = parse_soname(soname)
    symfiles = {symbols_path + libname + soversion + '.symbols'}
    symfiles.update(set(glob(symbols_path + '**.symbols')))

    for symfile in symfiles:
        try:
            metadata = yaml_load(symfile)
        except FileNotFoundError:
            continue
        if soname in metadata:
            version = _mmpack_lib_minversion(metadata[soname], symbols)
            return (metadata[soname]['depends'], version, Version('any'))

    raise FileNotFoundError('no installed mmpack package provides ' + soname)


class BinaryPackage:
    # pylint: disable=too-many-instance-attributes
    """
    Binary package class
    """

    def __init__(self, name: str, version: Version, source: str, arch: str,
                 tag: str, spec_dir: str):
        # pylint: disable=too-many-arguments
        self.name = name
        self.version = version
        self.source = source
        self.arch = arch
        self.tag = tag
        self.spec_dir = spec_dir
        self.src_hash = None
        self.pkg_path = None

        self.description = ''
        # * System dependencies are stored as opaque strings.
        #   Those are supposed to be handles by system tools,
        #   => format is a set of strings.
        # * mmpack dependencies are expressed as the triplet
        #   dependency name, min and max version (inclusive)
        #   => format is a dict {depname: [min, max], ...}
        self._dependencies = {'sysdepends': set(), 'depends': {}}
        self.provides = {'sharedlib': {}, 'python': {}}
        self.install_files = set()

    def _get_specs_provides(self) -> Dict[str, Dict[str, Version]]:
        """
        return a dict containing the specified interface of given package

        Look for a <self.name>.provides file within the project's mmpack
        folder, load it and return its parsed values as a dictionary.

        return an empty interface dict if none was specified.
        """
        # TODO: also work with the last package published
        provide_spec_name = '{}/{}.provides'.format(self.spec_dir, self.name)
        dprint('reading symbols from ' + provide_spec_name)
        specs_provides = dict()
        try:
            specs_provides.update(yaml_load(provide_spec_name))
        except FileNotFoundError:
            # return an empty dict if nothing has been provided
            pass

        return specs_provides

    def _symbol_file(self) -> str:
        os.makedirs('var/lib/mmpack/metadata/', exist_ok=True)
        return 'var/lib/mmpack/metadata/{}.symbols'.format(self.name)

    def _pyobjects_file(self) -> str:
        os.makedirs('var/lib/mmpack/metadata/', exist_ok=True)
        return 'var/lib/mmpack/metadata/{}.pyobjects'.format(self.name)

    def _sha256sums_file(self) -> str:
        os.makedirs('var/lib/mmpack/metadata/', exist_ok=True)
        return 'var/lib/mmpack/metadata/{}.sha256sums'.format(self.name)

    def _gen_info(self, pkgdir: str):
        """
        This generate the info file and sha256sums. It must be the last step
        before calling _make_archive().
        """
        pushdir(pkgdir)

        # Create file containing of hashes of all installed files
        cksums = {}
        for filename in glob('**', recursive=True):
            # skip folder and MMPACK/info
            if not isfile(filename) or filename == 'MMPACK/info':
                continue

            # Add file with checksum
            cksums[filename] = sha256sum(filename, follow_symlink=True)
        yaml_serialize(cksums, self._sha256sums_file(), use_block_style=True)

        # Create info file
        info = {'version': self.version,
                'source': self.source,
                'description': self.description,
                'srcsha256': self.src_hash,
                'sumsha256sums': sha256sum(self._sha256sums_file())}
        info.update(self._dependencies)
        yaml_serialize({self.name: info}, 'MMPACK/info')
        popdir()

    def _gen_symbols(self, pkgdir: str):
        pushdir(pkgdir)
        if self.provides['sharedlib']:
            yaml_serialize(self.provides['sharedlib'], self._symbol_file())
        popdir()

    def _gen_pyobjects(self, pkgdir: str):
        pushdir(pkgdir)
        if self.provides['python']:
            yaml_serialize(self.provides['python'], self._pyobjects_file())
        popdir()

    def _populate(self, instdir: str, pkgdir: str):
        for instfile in self.install_files:
            src = instdir + '/' + instfile
            dst = pkgdir + '/' + instfile
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.link(src, dst, follow_symlinks=False)

    def _make_archive(self, pkgdir: str, dstdir: str) -> str:
        mpkfile = "{0}/{1}_{2}_{3}.mpk".format(dstdir, self.name,
                                               self.version, self.arch)
        tar = tarfile.open(mpkfile, 'w:xz')
        dprint('[tar] {0} -> {1}'.format(pkgdir, mpkfile))
        tar.add(pkgdir, recursive=True, filter=_reset_entry_attrs, arcname='.')
        tar.close()

        return mpkfile

    def create(self, instdir: str, pkgbuilddir: str) -> str:
        """
        Gather all the package data, generates metadata files
        (including exposed symbols), and create the mmpack package
        file.

        Args:
            instdir: folder from which the package must be populated
            pkgbuilddir: build folder of source package. The staging file
                for the package as well as the created binary package file
                will be located in this folder.

        Returns:
            the path of the created binary package file (in pkgbuilddir)
        """
        stagedir = get_staging_dir(pkgbuilddir, self.name)
        os.makedirs(stagedir + '/MMPACK', exist_ok=True)

        dprint('link {0} in {1}'.format(self.name, stagedir))
        self._populate(instdir, stagedir)
        self._gen_symbols(stagedir)
        self._gen_pyobjects(stagedir)
        self._gen_info(stagedir)
        self.pkg_path = self._make_archive(stagedir, pkgbuilddir)
        return self.pkg_path

    def add_depend(self, name: str, minver: Version,
                   maxver: Version = Version('any')):
        """
        Add mmpack package as a dependency with a minimal version
        """
        dependencies = self._dependencies['depends']
        if name not in dependencies:
            dependencies[name] = [minver, maxver]
        else:
            curr_minver, curr_maxver = dependencies.get(name)
            minver = max(curr_minver, minver)
            maxver = min(curr_maxver, maxver)
            dependencies[name] = [minver, maxver]

    def add_sysdepend(self, opaque_dep: str):
        """
        Add a system dependencies to the binary package
        """
        self._dependencies['sysdepends'].add(opaque_dep)

    def _provides_symbol(self, symbol_type: str, symbol: str) -> str:
        for name, entry in self.provides[symbol_type].items():
            if symbol in entry['symbols'].keys():
                return name
        return None

    def gen_provides(self):
        """
        Go through the install files, look for what this package provides

        eg. scan elf libraries for symbols
            python files for top-level classes and functions
            ...

        This fills the class provides field dictionary
        It does not return any value.

        Raises:
            ValueError: if a specified version is invalid or if a symbol in
                        the spec file is not found provided by the package
        """
        specs_provides = self._get_specs_provides()

        for inst_file in self.install_files:
            if not is_dynamic_library(inst_file, self.arch):
                continue

            file_type = get_exec_fileformat(self.arch)
            fmt_mod = importlib.import_module(file_type + '_utils')
            soname = fmt_mod.soname(inst_file)
            entry = {soname: {'depends': self.name,
                              'symbols': {}}}
            entry[soname]['symbols'].update(
                fmt_mod.symbols_list(inst_file, self.version))
            self.provides['sharedlib'].update(entry)

        for symbol_type in self.provides:
            specs_symbols = specs_provides.get(symbol_type, dict())
            for symbol, str_version in specs_symbols.items():
                # type conversion will raise an error if malformed
                name = self._provides_symbol(symbol_type, symbol)
                if not name:
                    raise ValueError('Specified {0} symbol {1} not found '
                                     'in package files'
                                     .format(symbol_type, symbol))

                version = Version(str_version)
                if version <= self.version:
                    entry = self.provides[symbol_type][name]
                    entry['symbols'][symbol] = version
                else:  # version > self.version:
                    raise ValueError('Specified version of symbol {0} ({1})'
                                     'is greater than current version ({2})'
                                     .format(symbol, version, self.version))

    def _gen_lib_deps(self, soname: str, fileformat: str,
                      symbol_set: Set[str], binpkgs: List['BinaryPackages']):
        """
        Generate the library dependencies given a library used

        Args:
            soname: soname of library used
            fileformat: executable file format of the library used
            symbol_set: set of used symbols that are still unassociated
            binpkgs: the list of packages currently being generated
        """
        # provided in the same package or a package being generated
        for pkg in binpkgs:
            if soname in pkg.provides[fileformat]:
                for sym in pkg.provides[fileformat][soname]['symbols']:
                    symbol_set.discard(sym)
                if pkg.name != self.name:
                    self.add_depend(pkg.name, pkg.version, pkg.version)
                return

        # provided by one of the packages being generated at the same time
        for pkg in binpkgs:
            if soname in pkg.provides[fileformat]:
                self.add_depend(pkg.name, self.version, self.version)

                for sym in pkg.provides[fileformat][soname]:
                    symbol_set.discard(sym)
                return

        # provided by another mmpack package present in the prefix
        try:
            mmpack_dep, minv, maxv = _mmpack_lib_deps(soname, symbol_set)
            self.add_depend(mmpack_dep, minv, maxv)
            return

        # FileNotFoundError: invalid guessed symbol file name
        # KeyError: file does not contain soname
        except (FileNotFoundError, KeyError):
            pass

        # provided by the host system
        sysdep = _sysdep_find_dependency(soname, symbol_set)
        if not sysdep:
            # <soname> dependency could not be met with any available means
            errmsg = 'Could not find package providing ' + soname
            raise AssertionError(errmsg)

        self.add_sysdepend(sysdep)

    def _find_link_deps(self, target: str, binpkgs: List['BinaryPackages']):
        for pkg in binpkgs:
            if target in pkg.install_files and pkg.name != self.name:
                self.add_depend(pkg.name, pkg.version, pkg.version)

    def _find_dll_dep(self, import_lib: str, binpkgs: List['BinaryPackages']):
        """
        Adds to dependencies the package that hosts the dll associated with
        import library.
        """
        dll = get_linked_dll(import_lib)
        for pkg in binpkgs:
            pkg_dlls = [basename(f).lower() for f in pkg.install_files
                        if f.endswith(('.dll', '.DLL'))]
            if dll in pkg_dlls:
                if pkg.name != self.name:
                    self.add_depend(pkg.name, pkg.version, pkg.version)
                return

    def gen_dependencies(self, binpkgs: List['BinaryPackages']):
        """
        Go through the install files and search for dependencies.
        """
        deps = {'sharedlib': set(), 'python': set()}
        symbols = {'sharedlib': set(), 'python': set()}
        for inst_file in self.install_files:
            if os.path.islink(inst_file):
                target = os.path.join(os.path.dirname(inst_file),
                                      os.readlink(inst_file))
                self._find_link_deps(target, binpkgs)
                continue

            if is_importlib(inst_file):
                self._find_dll_dep(inst_file, binpkgs)
                continue

            file_type = filetype(inst_file)
            if file_type in ('elf', 'pe'):
                fmt_mod = importlib.import_module(file_type + '_utils')
                symbols['sharedlib'].update(
                    fmt_mod.undefined_symbols(inst_file))
                deps['sharedlib'].update(fmt_mod.soname_deps(inst_file))

        for fileformat in ('sharedlib', 'python'):
            for dep in deps[fileformat]:
                self._gen_lib_deps(dep, fileformat,
                                   symbols[fileformat], binpkgs)
                if not symbols[fileformat]:
                    break

        for fileformat in ('sharedlib', 'python'):
            if symbols[fileformat]:
                errmsg = 'Failed to process all {0} of {1} symbols\n' \
                         .format(fileformat, self.name)
                errmsg += 'Remaining symbols:\n\t'
                errmsg += '\n\t'.join(symbols[fileformat])
                raise AssertionError(errmsg)
