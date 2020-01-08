# @mindmaze_header@
"""
helper module containing dpkg files parsing functions
"""

import os
import re
import gzip

from glob import glob
from io import TextIOBase
from typing import List
from functools import partial

from . common import *
from . mm_version import Version
from . settings import DPKG_METADATA_PREFIX
from . syspkg_manager_base import SysPkgManager, SysPkg
from . workspace import cached_download


REPO_URL = 'http://debian.proxad.net/debian'


def dpkg_find_shlibs_file(target_soname: str):
    """
    Find shlibs file from soname.

    Returns:
      full path to the shlibs file

    Raises:
      FileNotFoundError: shlibs could not be found
    """
    name, version = parse_soname(target_soname)
    guess_pkgname = name + version
    shlib_soname_regex = re.compile(r'\b{0} {1}\b .*'.format(name, version))

    guess = glob(DPKG_METADATA_PREFIX + '/'
                 + guess_pkgname + '**.shlibs')
    if guess and os.path.exists(guess[0]):
        return guess[0]

    for shlibs_file in glob(DPKG_METADATA_PREFIX + '/**.shlibs'):
        if shlib_soname_regex.search(open(shlibs_file).read()):
            return shlibs_file

    errmsg = 'could not find dpkg shlibs file for ' + target_soname
    raise FileNotFoundError(errmsg)


def _prune_soname_symbols(library_path: str, symbols_list: List[str]):
    # to the import at the last moment in order to prevent windows
    # from failing to import elftools
    from . elf_utils import symbols_set
    for sym in symbols_set(library_path):
        if sym in symbols_list:
            symbols_list.remove(sym)


def dpkg_parse_shlibs(filename: str, target_soname: str,
                      symbols_list: List[str]) -> str:
    """
    Parse dpkg shlibs file.

    Returns:
        A dependency template

    Raises:
        Assert: symbols could not be found
    """
    dependency_template = None
    name, version = parse_soname(target_soname)
    shlib_soname = '{0} {1} '.format(name, version)
    for line in open(filename):
        line = line.strip('\n')
        if line.startswith(shlib_soname):
            dependency_template = line[len(shlib_soname):]
            break
    if not dependency_template:
        raise Assert(target_soname + 'not found in ' + filename)

    # read library file to prune symbols
    # assert the existence of <pkgname>*.list
    pkgname = dependency_template.split(' ')[0]
    glob_search_pattern = '{}/{}**:{}.list' \
                          .format(DPKG_METADATA_PREFIX,
                                  pkgname,
                                  get_host_arch())
    dpkg_list_file = glob(glob_search_pattern)[0]
    for line in open(dpkg_list_file):
        line = line.strip('\n')
        if target_soname in line:
            _prune_soname_symbols(line, symbols_list)

    return dependency_template


def dpkg_find_symbols_file(target_soname: str) -> str:
    """
    Find symbols file from soname.

    Returns:
      full path to the symbols file

    Raises:
      FileNotFoundError: symbols could not be found
    """
    name, version = parse_soname(target_soname)
    guess_pkgname = name + version

    # make sure the library characters are not interpreted as PCRE
    # Eg: libstdc++.so.6
    #            ^^^  ^  all those are PCRE wildcards
    # Note: '\b' is PCRE for word boundary
    symbols_soname_regex = re.compile(r'\b{0}\b .*'
                                      .format(re.escape(target_soname)))

    glob_search_pattern = '{}/{}**:{}.symbols' \
                          .format(DPKG_METADATA_PREFIX,
                                  guess_pkgname,
                                  get_host_arch())
    guess = glob(glob_search_pattern)
    if guess and os.path.exists(guess[0]):
        return guess[0]

    glob_search_pattern = '{}/**:{}.symbols' \
                          .format(DPKG_METADATA_PREFIX, get_host_arch())
    for symbols_file in glob(glob_search_pattern):
        if symbols_soname_regex.search(open(symbols_file).read()):
            return symbols_file

    errmsg = 'could not find dpkg symbols file for ' + target_soname
    raise FileNotFoundError(errmsg)


def dpkg_parse_symbols(filename: str, target_soname: str,
                       symbols_list: List[str]) -> str:
    """
    Parse dpkg symbols file.

    www.debian.org/doc/debian-policy/ch-sharedlibs.html#s-sharedlibs-symbols

    dpkg symbols file format:
        library-soname main-dependency-template
         [| alternative-dependency-template]
         [...]
         [* field-name: field-value]
         [...]
         symbol minimal-version [id-of-dependency-template]

    }

    Returns:
        A filled dependency template:
        - correct alternate dependency will have been chosen
        - #MINVER# will be filled
    """
    # TODO: find a way to split this (and thus satisty the two below ...)
    # pylint: disable=too-many-locals
    # pylint: disable=too-many-branches
    dependency_template = None
    templates = {}
    templates_cpt = 0
    minversion = None
    soname = None

    for line in open(filename):
        line = line.strip('\n')
        if line.startswith('*'):  # field name: field-value
            continue  # ignored
        elif line.startswith('|'):  # alternate dependency
            alt_dependency_template = line[2:]
            templates[templates_cpt] = alt_dependency_template
            templates_cpt += 1
        elif line.startswith(' '):  # symbol
            if not soname:
                continue

            line = line[1:]  # skip leading space
            split = line.split(' ')

            sym = split[0]
            if sym.endswith('@Base'):
                sym = sym[:-len('@Base')]
            if sym not in symbols_list:
                continue
            symbols_list.remove(sym)

            version = Version(split[1])
            if not minversion:
                minversion = version
            else:
                minversion = max(minversion, version)

            if len(split) == 3:
                alt_index = int(split[2])
                dependency_template = templates[alt_index]

        else:  # library-soname
            if soname:
                break

            read_soname = line.split(' ')[0]
            if read_soname != target_soname:
                continue

            soname = read_soname
            main_dependency_template = ' '.join(line.split(' ')[1:])
            templates[templates_cpt] = main_dependency_template
            templates_cpt += 1

            dependency_template = main_dependency_template

    if '#MINVER#' in dependency_template:
        if minversion:
            minver = '(>= {0})'.format(str(minversion))
        else:
            # may happen if linked without --as-needed flag:
            # the library is needed, but none of its symbols
            # are used
            minver = ''
        dependency_template = dependency_template.replace('#MINVER#', minver)

    return dependency_template.strip()


def dpkg_find_dependency(soname: str, symbol_list: List[str]) -> str:
    """
    Parses the debian system files, find a dependency template for soname
    """
    try:
        symbols_file = dpkg_find_symbols_file(soname)
        return dpkg_parse_symbols(symbols_file, soname, symbol_list)
    except FileNotFoundError:
        shlibs_file = dpkg_find_shlibs_file(soname)
        return dpkg_parse_shlibs(shlibs_file, soname, symbol_list)


def dpkg_find_pypkg(pypkg: str) -> str:
    """
    Get installed debian package providing the specified python package
    """
    # dpkg -S accept a glob-like pattern
    pattern = '/usr/lib/python3/dist-packages/{}[/|.py|.*.so]*'.format(pypkg)
    cmd_output = shell(['dpkg', '--search', pattern])
    debpkg_list = list({l.split(':')[0] for l in cmd_output.splitlines()})
    return debpkg_list[0] if debpkg_list else None


class DebPkg(SysPkg):
    """
    representation of a package in a Debian-based distribution repository
    """
    desc_field_to_attr = {
        'Package': 'name',
        'Version': 'version',
        'Source': 'source',
        'Filename': 'filename',
        'SHA256': 'sha256',
    }

    def get_sysdep(self) -> str:
        return '{} (>= {})'.format(self.name, self.version)

    @classmethod
    def fromindex(cls, fileobj: TextIOBase):
        """
        read next package from package index
        """
        pkg = DebPkg()
        for line in fileobj:
            try:
                key, data = line.split(':', 1)
                attrname = cls.desc_field_to_attr.get(key)
                if attrname:
                    pkg.__setattr__(attrname, data.strip())
            except ValueError:
                # Check end of paragraph
                if not line.strip():
                    if not pkg.source:
                        pkg.source = pkg.name
                    return pkg
        return None

    @classmethod
    def list_index(cls, fileobj: TextIOBase):
        """
        iterator over all packages described in a package index
        """
        return iter(partial(cls.fromindex, fileobj), None)


class Dpkg(SysPkgManager):
    """
    Class to interact with Debian package database
    """
    def find_sharedlib_sysdep(self, soname: str, symbols: List[str]) -> str:
        return dpkg_find_dependency(soname, symbols)

    def find_pypkg_sysdep(self, pypkg: str) -> str:
        return dpkg_find_pypkg(pypkg)

    def _get_mmpack_version(self, sys_version: str) -> Version:
        # remove epoch part if any, ie "epoch:version" and "version" return
        # "version"
        return Version(sys_version.split(':', 1)[-1])

    def _extract_syspkg(self, pkgfile: str, unpackdir: str) -> List[str]:
        files = shell(['dpkg-deb', '--vextract', pkgfile, unpackdir]).split()
        return [f.lstrip('./') for f in files]

    def _parse_pkgindex(self, builddir: str, srcname: str) -> List[SysPkg]:
        pkg_list = []

        arch = get_host_arch()
        gzipped_index = os.path.join(builddir, 'Packages.gz')
        pkgindex_url = '{}/dists/stable/main/binary-{}/Packages.gz'\
                       .format(REPO_URL, arch)

        # download compressed index
        cached_download(pkgindex_url, gzipped_index)

        # Parse compressed package index
        for pkg in DebPkg.list_index(gzip.open(gzipped_index, 'rt')):
            if pkg.source == srcname:
                pkg.url = REPO_URL + '/' + pkg.filename
                pkg.filename = os.path.basename(pkg.filename)
                pkg_list.append(pkg)

        return pkg_list
