# @mindmaze_header@
'''
Class to handle binary packages, their dependencies, symbol interface, and
packaging as mmpack file.
'''

import importlib
import os
from os.path import isfile
from glob import glob
from typing import List, Dict, Set
import tarfile
from common import *
from version import Version
import yaml
from dpkg import dpkg_find_dependency
from workspace import Workspace


def _reset_entry_attrs(tarinfo: tarfile.TarInfo):
    '''
    filter function for tar creation that will remove all file attributes
    (uid, gid, mtime) from the file added to tar would can make the build
    of package not reproducible.

    Args:
        tarinfo: entry being added to the tar
    Return:
        the modified tarinfo that will be actually added to tar
    '''
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = 'root'
    tarinfo.mtime = 0
    return tarinfo


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


class BinaryPackage(object):
    # pylint: disable=too-many-instance-attributes
    '''
    Binary package class
    '''
    def __init__(self, name: str, version: Version, source: str, arch: str,
                 tag: str):
        # pylint: disable=too-many-arguments
        self.name = name
        self.version = version
        self.source = source
        self.arch = arch
        self.tag = tag

        self.description = ''
        # * System dependencies are stored as opaque strings.
        #   Those are supposed to be handles by system tools,
        #   => format is a set of strings.
        # * mmpack dependencies are expressed as the triplet
        #   dependency name, min and max version (inclusive)
        #   => format is a dict {depname: [min, max], ...}
        self._dependencies = {'sysdepends': set(), 'depends': {}}
        self.provides = {'elf': {}, 'pe': {}, 'python': {}}
        self.install_files = set()

    def _get_specs_provides(self,
                            pkgname: str) -> Dict[str, Dict[str, Version]]:
        ' TODOC '
        # TODO: also work with the last package published
        wrk = Workspace()
        provide_spec_name = '{0}/{1}-{2}/mmpack/{3}.provides' \
                            .format(wrk.sources, self.source, self.tag,
                                    pkgname)
        dprint('reading symbols from ' + provide_spec_name)
        try:
            specs_provides = yaml_load(provide_spec_name)
        except FileNotFoundError:
            # return an empty dict if nothing has been provided
            specs_provides = {'elf': {}, 'pe': {}, 'python': {}}

        return specs_provides

    def _gen_info(self, pkgdir: str):
        '''
        This generate the info file and sha256sums. It must be the last step
        before calling _make_archive().
        '''
        pushdir(pkgdir)

        # Create file containing of hashes of all installed files
        cksums = {}
        for filename in glob('**', recursive=True):
            # skip folder and MMPACK/info
            if not isfile(filename) or filename == 'MMPACK/info':
                continue

            # Add file with checksum
            cksums[filename] = sha256sum(filename)
        yaml_serialize(cksums, 'MMPACK/sha256sums', use_block_style=True)

        # Create info file
        info = {'version': self.version,
                'source': self.source,
                'description': self.description,
                'sumsha256sums': sha256sum('MMPACK/sha256sums')}
        info.update(self._dependencies)
        yaml_serialize({self.name: info}, 'MMPACK/info')
        popdir()

    def _gen_symbols(self, pkgdir: str):
        pushdir(pkgdir)
        if self.provides['pe'] and self.provides['elf']:
            raise AssertionError(self.name + 'cannot contain symbols for two '
                                 'different architectures at the same time')
        if self.provides['pe']:
            yaml_serialize(self.provides['pe'], 'MMPACK/symbols')
        elif self.provides['elf']:
            yaml_serialize(self.provides['elf'], 'MMPACK/symbols')
        popdir()

    def _gen_pyobjects(self, pkgdir: str):
        pushdir(pkgdir)
        if self.provides['python']:
            yaml_serialize(self.provides['python'], 'MMPACK/pyobjects')
        popdir()

    def _populate(self, instdir: str, pkgdir: str):
        for instfile in self.install_files:
            src = instdir + instfile
            dst = pkgdir + '/' + instfile
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.link(src, dst, follow_symlinks=False)

    def _make_archive(self, pkgdir: str) -> str:
        wrk = Workspace()
        pkgdir = wrk.stagedir(self.name)
        mpkfile = "{0}/{1}_{2}_{3}.mpk".format(wrk.packages, self.name,
                                               self.version, self.arch)
        tar = tarfile.open(mpkfile, 'w:xz')
        dprint('[tar] {0} -> {1}'.format(pkgdir, mpkfile))
        tar.add(pkgdir, recursive=True, filter=_reset_entry_attrs, arcname='.')
        tar.close()

        return mpkfile

    def create(self, instdir: str) -> str:
        '''
        Gather all the package data, generates metadata files
        (including exposed symbols), and create the mmpack package
        file.
        '''
        wrk = Workspace()
        stagedir = wrk.stagedir(self.name)
        os.makedirs(stagedir + '/MMPACK', exist_ok=True)

        dprint('link {0} in {1}'.format(self.name, stagedir))
        self._populate(instdir, stagedir)
        self._gen_symbols(stagedir)
        self._gen_pyobjects(stagedir)
        self._gen_info(stagedir)
        return self._make_archive(instdir)

    def add_depend(self, name: str, minver: Version,
                   maxver: Version=Version('any')):
        '''
        Add mmpack package as a dependency with a minimal version
        '''
        dependencies = self._dependencies['depends']
        if name not in dependencies:
            dependencies[name] = [minver, maxver]
        else:
            curr_minver, curr_maxver = dependencies.get(name)
            minver = max(curr_minver, minver)
            maxver = min(curr_maxver, maxver)
            dependencies[name] = [minver, maxver]

    def add_sysdepend(self, opaque_dep: str):
        '''
        Add a system dependencies to the binary package
        '''
        self._dependencies['sysdepends'].add(opaque_dep)

    def _provides_symbol(self, symbol_type: str, symbol: str) -> str:
        for name, entry in self.provides[symbol_type].items():
            if symbol in entry['symbols'].keys():
                return name
        return None

    def gen_provides(self):
        ''' Go through the install files, look for what this package provides

        eg. scan elf libraries for symbols
            python files for top-level classes and functions
            ...

        This fills the class provides field dictionary
        It does not return any value.

        Raises:
            ValueError: if a specified version is invalid or if a symbol in
                        the spec file is not found provided by the package
        '''
        specs_provides = self._get_specs_provides(self.name)

        for inst_file in self.install_files:
            file_type = is_dynamic_library(inst_file)
            if file_type in ('elf', 'pe'):
                fmt_mod = importlib.import_module(file_type + '_utils')
                soname = fmt_mod.soname(inst_file)
                entry = {soname: {'depends': self.name,
                                  'symbols': {}}}
                entry[soname]['symbols'].update(
                    fmt_mod.symbols_list(inst_file, self.version))
                self.provides[file_type].update(entry)
            else:
                if file_type == 'pe' or file_type == 'python':
                    dprint('[WARN] skipping file: ' + inst_file)

        for symbol_type in specs_provides:
            for symbol, str_version in specs_provides[symbol_type].items():
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
        '''
            Args:
                binpkgs: the list of packages currently being generated
        '''
        # provided in the same package
        if soname in self.provides[fileformat]:
            for sym in self.provides[fileformat][soname]:
                symbol_set.discard(sym)
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
        # TODO: switch on the host package manager
        dpkg_dep = dpkg_find_dependency(soname, symbol_set)
        if not dpkg_dep:
            # <soname> dependency could not be met with any available means
            errmsg = 'Could not find package providing ' + soname
            raise AssertionError(errmsg)

        self.add_sysdepend(dpkg_dep)

    def _find_link_deps(self, target: str, binpkgs: List['BinaryPackages']):
        for pkg in binpkgs:
            if target in pkg.install_files and pkg.name != self.name:
                self.add_depend(pkg.name, pkg.version, pkg.version)

    def gen_dependencies(self, binpkgs: List['BinaryPackages']):
        'Go through the install files and search for dependencies.'
        deps = {'elf': set(), 'pe': set(), 'python': set()}
        symbols = {'elf': set(), 'pe': set(), 'python': set()}
        for inst_file in self.install_files:
            if os.path.islink(inst_file):
                target = os.path.join(os.path.dirname(inst_file),
                                      os.readlink(inst_file))
                self._find_link_deps(target, binpkgs)
                continue

            file_type = filetype(inst_file)
            if file_type in ('elf', 'pe'):
                fmt_mod = importlib.import_module(file_type + '_utils')
                symbols[file_type].update(fmt_mod.undefined_symbols(inst_file))
                deps[file_type].update(fmt_mod.soname_deps(inst_file))

        for fileformat in ('elf', 'pe', 'python'):
            for dep in deps[fileformat]:
                self._gen_lib_deps(dep, fileformat,
                                   symbols[fileformat], binpkgs)
                if not symbols[fileformat]:
                    break

        for fileformat in ('elf', 'pe', 'python'):
            if symbols[fileformat]:
                errmsg = 'Failed to process all {0} of {1} symbols\n' \
                         .format(fileformat, self.name)
                errmsg += 'Remaining symbols:\n\t'
                errmsg += '\n\t'.join(symbols[fileformat])
                raise AssertionError(errmsg)
