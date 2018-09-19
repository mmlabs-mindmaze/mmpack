# @mindmaze_header@
'''
Class to handle binary packages, their dependencies, symbol interface, and
packaging as mmpack file.
'''

import os
from os.path import isfile
from glob import glob
from typing import Dict
import tarfile
from common import sha256sum, yaml_serialize, pushdir, popdir, dprint, \
         filetype
from version import Version
from dependencies import scan_dependencies
from elf_utils import elf_symbols_list, elf_soname
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
        self._dependencies = {'sysdepends': {}, 'depends': {}}
        self._provides = {'elf': {}, 'pe': {}, 'python': {}}
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

    def _populate(self, instdir: str, pkgdir: str) -> None:
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
        # TODO: generate a metadata file with the list of symbols provided
        return self._make_archive(instdir)

    def add_depend(self, name: str, version: Version) -> None:
        '''
        Add mmpack package as a dependency with a minimal version
        '''
        dependencies = self._dependencies['depends']
        curr_version = dependencies.get(name)
        if not curr_version or curr_version < version:
            dependencies[name] = version

    def add_sysdepend(self, name: str, version: Version) -> None:
        '''
        Add a system dependencies to the binary package
        '''
        dependencies = self._dependencies['sysdepends']
        curr_version = dependencies.get(name)
        if not curr_version or curr_version < version:
            dependencies[name] = version

    def _provides_symbol(self, symbol_type: str, symbol: str) -> str:
        for name, entry in self._provides[symbol_type].items():
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
            file_type = filetype(inst_file)
            if file_type == 'elf':
                soname = elf_soname(inst_file)
                entry = {soname: {'depends': self.name,
                                  'symbols': {}}}
                entry[soname]['symbols'].update(elf_symbols_list(inst_file,
                                                                 self.version))
                self._provides['elf'].update(entry)
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
                    entry = self._provides[symbol_type][name]
                    entry['symbols'][symbol] = version
                else:  # version > self.version:
                    raise ValueError('Specified version of symbol {0} ({1})'
                                     'is greater than current version ({2})'
                                     .format(symbol, version, self.version))

    def gen_sysdeps(self) -> None:
        ''' Go through the install files and search for external dependencies

        FIXME: only handle elf deps ...
        '''
        for inst_file in self.install_files:
            dep = scan_dependencies(inst_file)
            if dep:
                self._dependencies['sysdepends'].update(dep)
