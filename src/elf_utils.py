#!/usr/bin/env python3
'''
helper module containing elf parsing functions
'''

import subprocess

from elftools.common.exceptions import ELFError
from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection

from common import remove_duplicates


def elf_deps(filename):
    ''' Parse given elf file and return its dependency *library* list
    '''
    elffile = ELFFile(open(filename, 'rb'))

    librarylist = []
    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_NEEDED':
                    librarylist.append(tag.needed)

    return sorted(librarylist)


def elf_symbols_list(filename, default_version):
    ''' Parse given elf file and return its exported symbols as a dictionary.
        dict keys are symbols name, values will be the symbols version
        they are initialized unset (None) in this function
    '''
    try:
        elffile = ELFFile(open(filename, 'rb'))
    except (IsADirectoryError, ELFError):
        return {}

    symbols = {}
    dyn = elffile.get_section_by_name('.dynsym')
    for sym in dyn.iter_symbols():
        if (sym['st_info']['bind'] == 'STB_GLOBAL' and
                sym['st_other']['visibility'] == 'STV_PROTECTED'):
            symbols[sym.__dict__['name']] = default_version

    return symbols


def elf_pkg_deps(filename):
    'Parse given elf file and return its dependency *package* list'
    packagelist = []
    librarylist = elf_deps(filename)
    for lib in librarylist:
        cmd = ['dpkg', '--search'] + [lib]
        ret = subprocess.run(cmd, check=False, stdout=subprocess.PIPE)
        if ret.returncode == 0:
            for package in ret.stdout.decode('utf-8').split('\n'):
                try:
                    pkg, arch = package.split(':')[0:2]
                    if arch in ['amd64', 'x86_64']:
                        packagelist.append(pkg)
                except ValueError:
                    continue

    remove_duplicates(packagelist)
    return packagelist


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
