# @mindmaze_header@
"""
helper module containing elf parsing functions
"""

import os
from typing import List

from elftools.common.exceptions import ELFError
from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection

from . common import shell


def _get_runpath_list(filename) -> List[str]:
    elffile = ELFFile(open(filename, 'rb'))

    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_RUNPATH':
                    return tag.runpath.split(':')

    return []


def adjust_runpath(filename):
    """
    Adjust the DT_RUNPATH value of filename so that library dependencies in
    mmpack prefix can be found and loaded at runtime, no matter the location of
    the prefix. If DT_RUNPATH field was not empty, the path is only added to
    the previous content (which will have precedence over what has been added).
    """
    mmpack_comp = '/run/mmpack/lib'  # We are on ELF, so always /run/mmpack

    # append the component needed for mmpack if not found in current list of
    # DT_RUNPATH path components
    runpath = _get_runpath_list(filename)
    if mmpack_comp not in runpath:
        runpath.append(mmpack_comp)
        shell(['patchelf', '--set-rpath', ':'.join(runpath), filename])


def soname_deps(filename):
    """
    Parse given elf file and return its dependency soname list
    """
    elffile = ELFFile(open(filename, 'rb'))

    libraryset = set()
    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_NEEDED':
                    libraryset.add(tag.needed)

    return libraryset


def undefined_symbols(filename):
    """
    Parse given elf file and return its undefined symbols set

    We require 3 sections for the elf file:
        - .gnu.version_r: lists the needed libraries version
        - .gnu.version: contains an index into .gnu.version_r:
        - .dynsym: contains the list of all symbols

    If the nth symbol from .dynsym has an entry (nth -> index)
    the the symbol name is required to be of version .gnu.version_r[index]

    This is the reason why we need to iterate over all symbols with enumerate()
    """
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


def soname(filename: str) -> str:
    """
    Returns:
        the SONAME of given library

    Raises:
        ELFError: soname not found
    """
    elffile = ELFFile(open(filename, 'rb'))
    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_SONAME':
                    return tag.soname
    libname = os.path.basename(filename)
    raise ELFError('SONAME not found in library: ' + libname)


def symbols_set(filename):
    """
    Parse given elf file and return its exported symbols as a set.
    """
    try:
        elffile = ELFFile(open(filename, 'rb'))
    except (IsADirectoryError, ELFError):
        return {}

    symbols = set()
    dyn = elffile.get_section_by_name('.dynsym')
    for sym in dyn.iter_symbols():
        if (sym['st_info']['bind'] == 'STB_GLOBAL'
                and sym['st_size'] != 0
                and (sym['st_other']['visibility'] == 'STV_PROTECTED'
                     or sym['st_other']['visibility'] == 'STV_DEFAULT')
                and sym['st_shndx'] != 'SHN_UNDEF'):
            symbols.add(sym.name)

    return symbols
