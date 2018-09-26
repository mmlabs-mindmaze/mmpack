# @mindmaze_header@
'''
Class to handle binary packages, their dependencies, symbol interface, and
packaging as mmpack file.
'''

import os
from os.path import isfile
from glob import glob
from typing import List, Dict
import tarfile
from common import sha256sum, yaml_serialize, pushdir, popdir, dprint, \
         filetype, remove_duplicates, is_dynamic_library
from dpkg import dpkg_find_dependency
from version import Version
from elf_utils import elf_symbols_list, elf_soname, elf_undefined_symbols, \
         elf_soname_deps
import yaml
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


def _mmpack_elf_minversion(soname: str, metadata_file: str,
                           symbols: List[str]) -> Version:
    min_version = None
    metadata = yaml.load(open(metadata_file, 'rb').read())
    for sym in list(symbols):  # iterate over a shallow copy of the list
        if sym in metadata[soname]['symbols']:
            metadata_version = Version(metadata[soname]['symbols'][sym])
            if min_version:
                min_version = max(min_version, metadata_version)
            else:
                min_version = metadata_version
            symbols.remove(sym)  # remove symbol from the list

    return min_version


def _mmpack_elf_deps(soname: str,
                     symbols: List[str]) -> (str, Version, Version):
    wrk = Workspace()

    pkg_name_guess = soname.split('.')[0] + soname.split('.')[2]
    symbols_path = wrk.prefix + '/var/lib/mmpack/metadata/'
    mmpack_installed_file = wrk.prefix + '/var/lib/mmpack/installed.yaml'
    try:  # guess the package name from the soname
        symbols_filename = symbols_path + pkg_name_guess + '.symbols'
        version = _mmpack_elf_minversion(soname, symbols_filename, symbols)
        return (pkg_name_guess, version, Version('any'))
    except FileNotFoundError:
        symfiles = glob(symbols_path + '**.symbols')
        mmpack_installed = yaml.load(open(mmpack_installed_file, 'rb').read())
        for pkgname in mmpack_installed:
            symbols_filename = symbols_path + pkgname + '.symbols'
            if symbols_filename in symfiles:
                version = _mmpack_elf_minversion(soname, symbols_filename,
                                                 symbols)
                return (pkgname, version, Version('any'))


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
        #   => format is a list of strings.
        # * mmpack dependencies are expressed as the triplet
        #   dependency name, min and max version (inclusive)
        #   => format is a dict {depname: [min, max], ...}
        self._dependencies = {'sysdepends': [], 'depends': {}}
        self.provides = {'elf': {}, 'pe': {}, 'python': {}}
        self.install_files = []

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
            specs_provides = yaml.load(open(provide_spec_name, 'rb').read())
        except FileNotFoundError:
            # return an empty dict if nothing has been provided
            specs_provides = {'elf': {}, 'pe': {}, 'python': {}}

        return specs_provides

    def _gen_info(self, pkgdir: str):
        pushdir(pkgdir)

        # Create file containing of hashes of all installed files
        cksums = {}
        for filename in glob('**', recursive=True):
            # skip folder and MMPACK/info
            if not isfile(filename) or filename == 'MMPACK/info':
                continue

            # Add file with checksum
            cksums[filename] = sha256sum(filename)
        yaml_serialize(cksums, 'MMPACK/sha256sums')

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
        self._gen_info(stagedir)
        self._gen_symbols(stagedir)
        self._gen_pyobjects(stagedir)
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
        dependencies = self._dependencies['sysdepends']
        if opaque_dep not in dependencies:
            dependencies.append(opaque_dep)

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
            if file_type == 'elf':
                soname = elf_soname(inst_file)
                entry = {soname: {'depends': self.name,
                                  'symbols': {}}}
                entry[soname]['symbols'].update(elf_symbols_list(inst_file,
                                                                 self.version))
                self.provides['elf'].update(entry)
            else:
                if file_type == 'pe' or file_type == 'python':
                    dprint('[WARN] skipping file: ' + inst_file)

        for symbol_type in specs_provides:
            for symbol, str_version in specs_provides[symbol_type].items():
                # type conversion will raise an error if malformed
                name = self._provides_symbol(symbol_type, symbol)
                if not name:
                    raise ValueError('Specified elf symbol {0} not found '
                                     'in package files'.format(symbol))

                version = Version(str_version)
                if version <= self.version:
                    entry = self.provides[symbol_type][name]
                    entry['symbols'][symbol] = version
                else:  # version > self.version:
                    raise ValueError('Specified version of symbol {0} ({1})'
                                     'is greater than current version ({2})'
                                     .format(symbol, version, self.version))

    def _gen_elf_deps(self, soname: str, symbol_list: List[str],
                      binpkgs: List['BinaryPackages']):
        '''
            Args:
                binpkgs: the list of packages currently being generated
        '''
        # provided in the same package
        if soname in self.provides['elf']:
            for sym in self.provides['elf'][soname]:
                if sym in symbol_list:
                    symbol_list.remove(sym)
            return

        # provided by one of the packages being generated at the same time
        for pkg in binpkgs:
            if soname in pkg.provides['elf']:
                self.add_depend(pkg.name, self.version, self.version)

                for sym in pkg.provides['elf'][soname]:
                    if sym in symbol_list:
                        symbol_list.remove(sym)
                return

        # provided by another mmpack package present in the prefix
        try:
            mmpack_dep, minv, maxv = _mmpack_elf_deps(soname, symbol_list)
            self.add_depend(mmpack_dep, minv, maxv)
            return
        except FileNotFoundError:
            pass

        # provided by the host system
        dpkg_dep = dpkg_find_dependency(soname, symbol_list)
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
        deps = []
        symbols = []
        for inst_file in self.install_files:
            if os.path.islink(inst_file):
                target = os.path.join(os.path.dirname(inst_file),
                                      os.readlink(inst_file))
                self._find_link_deps(target, binpkgs)
                continue

            file_type = filetype(inst_file)
            if file_type == 'elf':
                symbols += elf_undefined_symbols(inst_file)
                deps += elf_soname_deps(inst_file)

        remove_duplicates(deps)
        remove_duplicates(symbols)
        for dep in deps:
            self._gen_elf_deps(dep, symbols, binpkgs)
            if not symbols:
                break

        if symbols:
            errmsg = 'Failed to process all of ' + self.name + ' symbols\n'
            errmsg += 'Remaining symbols:\n\t'
            errmsg += '\n\t'.join(symbols)
            raise AssertionError(errmsg)
