# @mindmaze_header@
"""
helper module containing elf parsing functions
"""

import os
from typing import List, Optional

from elftools.common.exceptions import ELFError
from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection
from elftools.elf.sections import NoteSection

from .common import shell, wprint
from .provide import Provide, ProvidedSymbol


def _subpath_in_prefix(prefix: str, path: str) -> str:
    """
    Determine the subpath in a prefix when a path is inside the prefix

    Args:
        prefix: absolute path of a prefix
        path: path to test

    Returns: subpath within `prefix` if `path` is a subfolder of `prefix`.
    Otherwise None is returned.
    """
    try:
        # Although it looks simpler to use "if path.startswith(prefix)", this
        # would not cover many corner cases handled by the current approach
        if os.path.commonpath([prefix, path]) == prefix:
            subpath = path[len(prefix)+1:]
            return subpath if subpath else '.'
    except ValueError:
        pass

    return None


def _path_relative_to_origin(target_dir: str, start_dir: str) -> str:
    """
    Determine the component corresponding to the relative path from folder of
    filename to target_dir folder. `target_dir` and `start_dir` must be both
    relative to the same root folder.

    Args:
        target_dir: folder target folder to which the path must be computed
        start_dir: path of origin

    Returns: relative path from `start_dir` to `target_dir`. The returned
    string will necessarily begin with '$ORIGIN'.
    """
    to_target = os.path.relpath(target_dir, start_dir)
    new_path = '$ORIGIN'
    if to_target != '.':
        new_path += '/' + to_target

    return new_path


def _get_runpath_list(filename) -> List[str]:
    elffile = ELFFile(open(filename, 'rb'))

    for section in elffile.iter_sections():
        if isinstance(section, DynamicSection):
            for tag in section.iter_tags():
                if tag.entry.d_tag == 'DT_RUNPATH':
                    return tag.runpath.split(':')

    return []


def build_id(filename: str) -> Optional[str]:
    """
    return build id note. Returns None if not found.
    """
    with open(filename, 'rb') as fileobj:
        elffile = ELFFile(fileobj)
        for section in elffile.iter_sections():
            if isinstance(section, NoteSection):
                for note in section.iter_notes():
                    if note.n_type == 'NT_GNU_BUILD_ID':
                        return note.n_desc

    return None


def _has_dynamic_section(filename) -> bool:
    """
    return whether the input filename is an elf file with a dynamic section
    """
    try:
        elffile = ELFFile(open(filename, 'rb'))
        dyn = elffile.get_section_by_name('.dynamic')
        return dyn is not None
    except (IsADirectoryError, ELFError):
        return False


def adjust_runpath(filename):
    """
    Adjust the DT_RUNPATH value of filename so that library dependencies in
    mmpack prefix can be found and loaded at runtime, no matter the location of
    the prefix. If DT_RUNPATH field was not empty, the path is only added to
    the previous content (which will have precedence over what has been added).

    Additionally all absolute path elements that point to /run/mmpack will be
    transformed in paths relative to '$ORIGIN'.
    """
    if not _has_dynamic_section(filename):
        return

    runpath = _get_runpath_list(filename)
    filedir = os.path.dirname(filename)

    # Turn each element in runpath list that is an absolute path and points in
    # /run/mmpack into a relative path starting from file location. This makes
    # binary using those runpath usable even without running "mmpack run"
    for i, comp in enumerate(runpath):
        subcomp = _subpath_in_prefix('/run/mmpack', comp)
        if subcomp:
            runpath[i] = _path_relative_to_origin(subcomp, filedir)

    # append the component needed for mmpack if not found in current list of
    # DT_RUNPATH path components
    mmpack_comp = _path_relative_to_origin('lib', filedir)
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


def _get_version_table(elffile):
    version = {}
    gnu_version = elffile.get_section_by_name('.gnu.version')
    if not gnu_version:
        # TODO: parse unversioned symbols from .symtab/.dynsym
        msg = 'Could not find Symbol Version Table in {}' \
              .format(elffile.stream.name)
        wprint(msg)
        return {}
    for nsym, sym in enumerate(gnu_version.iter_symbols()):
        index = sym['ndx']
        if index not in ('VER_NDX_LOCAL', 'VER_NDX_GLOBAL'):
            index = int(index)
            # In GNU versioning mode, the highest bit is used to
            # store whether the symbol is hidden or not
            if index & 0x8000:
                index &= ~0x8000
            version[nsym] = index

    return version


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

    version = _get_version_table(elffile)
    gnu_version_r = elffile.get_section_by_name('.gnu.version_r')
    if not gnu_version_r:
        # TODO: parse unversioned symbols from .symtab/.dynsym
        msg = 'Could not find Version Requirement Table in {}'.format(elffile)
        wprint(msg)
        return {}

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

    # https://lists.debian.org/lsb-spec/1999/12/msg00017.html
    For each public symbol:
        1.  find its version index in the version table
            read in the .gnu.version section
        2.1 (only if the library's symbols are versioned)
            the index from (1.) is a pointer into a list of
            symbols version geven into section .gnu.version_d
        2.2 use that second index to recover the version string
        3.  use the version and the name of the symbol to create
            the full version name
    """
    try:
        elffile = ELFFile(open(filename, 'rb'))
    except (IsADirectoryError, ELFError):
        return {}

    symbols_versions = dict()

    # 1. get a table of version indexes
    version_table = _get_version_table(elffile)

    # 2.1 try to get a table of the possible versions
    ver_def = elffile.get_section_by_name('.gnu.version_d')
    if ver_def:
        for entry in ver_def.iter_versions():
            version, aux_iter = entry
            index = version['vd_ndx']
            for aux in aux_iter:
                name = aux.name
                break  # ignore parent entry (if any)
            symbols_versions[index] = name

    symbols = set()
    dyn = elffile.get_section_by_name('.dynsym')
    for nsym, sym in enumerate(dyn.iter_symbols()):
        if (sym['st_info']['bind'] in {'STB_GLOBAL', 'STB_WEAK'}
                and sym['st_size'] != 0
                and (sym['st_other']['visibility'] == 'STV_PROTECTED'
                     or sym['st_other']['visibility'] == 'STV_DEFAULT')
                and sym['st_shndx'] != 'SHN_UNDEF'):

            version = ''
            # we have an exported symbol
            # try 2.2 if possible
            if nsym in version_table:
                index = version_table[nsym]
                version = symbols_versions.get(index, '')

            if version:
                # 3. create the full version string
                symbols.add('{}@{}'.format(sym.name, version))
            else:
                symbols.add(sym.name)

    return symbols


def sym_basename(name: str) -> str:
    """
    return the base name of a symbol

    A symbol name may be composed of a name and a version glued together by
    the '@' character. This strips the version from the name.

    eg.
        xxx -> xxx
        xxx@yyy -> xxx
        xxx@yyy@zzz -> 'xxx@yyy
    """
    i = name.rfind('@')
    if i > 0:
        return name[:i]

    return name


class ShlibProvide(Provide):
    """
    Specialized Provide class which strips the version part of the symbol name.
    """
    def _get_decorated_symbols(self) -> List[ProvidedSymbol]:
        return [ProvidedSymbol(name=sym_basename(s), symbol=s)
                for s in self.symbols.keys()]
