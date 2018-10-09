# @mindmaze_header@
'''
helper module containing elf parsing functions
'''

import os
import subprocess

from elftools.common.exceptions import ELFError
from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection

from common import shell


def elf_soname_deps(filename):
    'Parse given elf file and return its dependency soname list'
    elffile = ELFFile(open(filename, 'rb'))

    libraryset = set()
    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_NEEDED':
                    libraryset.add(tag.needed)

    return libraryset


def elf_undefined_symbols(filename):
    '''Parse given elf file and return its undefined symbols set

    We require 3 sections for the elf file:
        - .gnu.version_r: lists the needed libraries version
        - .gnu.version: contains an index into .gnu.version_r:
        - .dynsym: contains the list of all symbols

    If the nth symbol from .dynsym has an entry (nth -> index)
    the the symbol name is required to be of version .gnu.version_r[index]

    This is the reason why we need to iterate over all symbols with enumerate()
    '''
    elffile = ELFFile(open(filename, 'rb'))

    undefined_symbols_set = set()

    gnu_version_r = elffile.get_section_by_name('.gnu.version_r')

    version = {}
    gnu_version = elffile.get_section_by_name('.gnu.version')
    for nsym, sym in enumerate(gnu_version.iter_symbols()):
        index = sym['ndx']
        if index not in ('VER_NDX_LOCAL', 'VER_NDX_GLOBAL'):
            index = int(index)
            # In GNU versioning mode, the highest bit is used to
            # store whether the symbol is hidden or not
            if index & 0x8000:
                index &= ~0x8000
            version[nsym] = index

    dyn = elffile.get_section_by_name('.dynsym')
    for nsym, sym in enumerate(dyn.iter_symbols()):
        if (sym['st_info']['bind'] == 'STB_GLOBAL'
                and sym['st_shndx'] == 'SHN_UNDEF'):
            symbol_str = sym.name
            if nsym in version:
                index = version[nsym]
                version_name = gnu_version_r.get_version(index)[1].name
                # objdump and readelf note this as <name>@@<version>
                # debian notes this with only a single @ in between
                symbol_str += '@' + version_name
            undefined_symbols_set.add(symbol_str)

    return undefined_symbols_set


def elf_soname(filename: str) -> str:
    ''' Return the SONAME of given library

    Raises:
        ELFError: soname not found
    '''
    elffile = ELFFile(open(filename, 'rb'))
    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_SONAME':
                    return tag.soname
    libname = os.path.basename(filename)
    raise ELFError('SONAME not found in library: ' + libname)


def elf_symbols_list(filename, default_version):
    ''' Parse given elf file and return its exported symbols as a dictionary.
        dict keys are symbols name, values will be the symbols version
    '''
    try:
        elffile = ELFFile(open(filename, 'rb'))
    except (IsADirectoryError, ELFError):
        return {}

    symbols = {}
    dyn = elffile.get_section_by_name('.dynsym')
    for sym in dyn.iter_symbols():
        if (sym['st_info']['bind'] == 'STB_GLOBAL'
                and sym['st_size'] != 0
                and (sym['st_other']['visibility'] == 'STV_PROTECTED'
                     or sym['st_other']['visibility'] == 'STV_DEFAULT')
                and sym['st_shndx'] != 'SHN_UNDEF'):
            symbols[sym.name] = default_version

    return symbols


def _dpkg_get_pkg_version(dpkg_name: str) -> str:
    cmd = 'dpkg --status {} | grep "^Version:"'.format(dpkg_name)
    version_line = shell(cmd)
    return version_line[len('Version:'):].strip()


def elf_dpkg_deps(filename):
    'Parse given elf file and return its dependency debian package dict'
    packagedict = {}
    librarylist = elf_soname_deps(filename)
    for lib in librarylist:
        cmd = ['dpkg', '--search'] + [lib]
        ret = subprocess.run(cmd, check=False, stdout=subprocess.PIPE)
        if ret.returncode == 0:
            for package in ret.stdout.decode('utf-8').split('\n'):
                try:
                    pkg, arch = package.split(':')[0:2]
                    if arch in ['amd64', 'x86_64']:
                        version = _dpkg_get_pkg_version(':'.join([pkg, arch]))
                        packagedict.update({pkg: version})
                except ValueError:
                    continue

    return packagedict


def elf_build_id(libname):
    ''' Parse given elf file and return its build-id

        Note: if compiled with gcc, default is to pass --build-id=uuid to the
        linker. However, with this is not the case and the build-id section
        is not present by default and must be added manually.

        Raises:
            Exception if no build-id section is found within the elf file
    '''
    with open(libname, 'rb') as libfile:
        elffile = ELFFile(libfile)
    dyn = elffile.get_section_by_name('.note.gnu.build-id')

    for note in dyn.iter_notes():
        if note['n_type'] == 'NT_GNU_BUILD_ID':
            return note['n_desc']

    raise Exception('build-id not found within library: {0}'.format(libname))
