# @mindmaze_header@
"""
helper module containing dpkg files parsing functions
"""

import os
import re

from email import message_from_bytes
from glob import glob
from importlib import import_module
from typing import List, TextIO, Iterator, Optional

from . common import *
from . mm_version import Version
from . settings import DPKG_METADATA_PREFIX
from . syspkg_manager_base import SysPkgManager, SysPkg
from . workspace import cached_download


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
    symbols_set = import_module('..elf_utils', __name__).symbols_set
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

        if line.startswith('|'):  # alternate dependency
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
    def get_sysdep(self) -> str:
        return '{} (>= {})'.format(self.name, self.version)


def _list_debpkg_index(fileobj: TextIO) -> Iterator[DebPkg]:
    desc_field_to_attr = {
        'Package': 'name',
        'Version': 'version',
        'Source': 'source',
        'Filename': 'filename',
        'SHA256': 'sha256',
    }

    pkg = DebPkg()
    for line in fileobj:
        try:
            key, data = line.split(':', 1)
            attrname = desc_field_to_attr.get(key)
            if attrname:
                setattr(pkg, attrname, data.strip())
        except ValueError:
            # Check end of paragraph
            if not line.strip():
                if not pkg.source:
                    pkg.source = pkg.name
                pkg.source = pkg.source.split(' ', 1)[0]
                yield pkg
                pkg = DebPkg()


class DebRepo:
    """
    Wrapper of Debian repository
    """
    def __init__(self, url: str, dist: str, builddir: str,
                 arch: Optional[str] = None):
        self.components = []
        self.baseurl = url
        self.dist = dist
        self.arch = arch if arch else get_host_arch()
        self.pkgs_index_list = []

        self._update_pkgs_index_list(builddir)

    def _update_pkgs_index_list(self, builddir: str):
        dist_url = f'{self.baseurl}/dists/{self.dist}/Release'
        release = message_from_bytes(get_http_req(dist_url).data)
        archs = release['Architectures'].split()
        components = release['Components'].split()
        shalist = {c[2]: (c[0], c[1]) for c in
                   [l.strip().split()
                    for l in release['SHA256'].strip().split('\n')]}

        if self.arch not in archs:
            raise RuntimeError(f'No dist for {self.arch} in {dist_url}')

        for comp in components:
            for ext in ('.gz', '.xz', ''):
                comp_res = f'{comp}/binary-{self.arch}/Packages{ext}'
                if comp_res in shalist:
                    comp_url = f'{self.baseurl}/dists/{self.dist}/{comp_res}'
                    filename = os.path.join(builddir,
                                            comp_res.replace('/', '_'))
                    cached_download(comp_url, filename,
                                    expected_sha256=shalist[comp_res][0])
                    self.pkgs_index_list.append(filename)
                    return

    def pkgs(self) -> Iterator[DebPkg]:
        """
        Iterator of package in the distribution
        """
        for index in self.pkgs_index_list:
            with open_compressed_file(index) as fileobj:
                for pkg in _list_debpkg_index(fileobj):
                    yield pkg


def _get_repo(srcnames: List[str]) -> (str, str):
    repo_str = os.getenv('MMPACK_BUILD_DPKG_REPO')
    if repo_str:
        return repo_str.split()

    for srcname in srcnames:
        cmd_output = shell(['apt-cache', 'madison', srcname])
        if cmd_output:
            break

    if not cmd_output:
        raise ValueError

    # Get in the madison command output the first source package
    for line in cmd_output.splitlines():
        if not line.endswith(' Sources'):
            continue
        debrepo = line.split('|')[-1].strip()
        base_url, distcomp, _ = debrepo.split()
        dist = distcomp.rsplit('/', maxsplit=1)[0]
        return (base_url, dist)

    # If we reach here, there was no source package called srcname
    raise ValueError


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
        version = sys_version.split(':', 1)[-1]
        # Remove distribution specific revision number
        return Version(version.rsplit('-', 1)[0])

    def _extract_syspkg(self, pkgfile: str, unpackdir: str) -> List[str]:
        cmdout = shell(['dpkg-deb', '--vextract', pkgfile, unpackdir])
        files = cmdout.splitlines()

        # Remove Debian specific data
        debdoc_re = re.compile(r'./usr/share/doc/.*/'
                               r'(AUTHORS|README|NEWS|changelog|copyright)'
                               r'(\.Debian|\.source|\.rst)?(\.gz)?')
        to_remove = {f for f in files if debdoc_re.fullmatch(f)
                     or f.startswith('./usr/share/lintian/overrides')}

        for filename in to_remove:
            fullpath = os.path.join(unpackdir, filename)
            if not os.path.isdir(fullpath):
                os.remove(fullpath)
                files.remove(filename)

        return [f.lstrip('./') for f in files]

    def _parse_pkgindex(self, builddir: str,
                        srcnames: List[str]) -> List[SysPkg]:
        # Try to get the repo that provide the specified source package
        try:
            repo_url, dist = _get_repo(srcnames)
        except ValueError:
            return []

        repo = DebRepo(repo_url, dist, builddir)
        pkg_list = []
        for srcname in srcnames:
            # Parse compressed package index for binary package matching source
            for pkg in repo.pkgs():
                if pkg.source == srcname:
                    # Skip debug packages
                    if pkg.name.endswith('-dbg'):
                        continue
                    pkg.url = repo_url + '/' + pkg.filename
                    pkg.filename = os.path.basename(pkg.filename)
                    pkg_list.append(pkg)

            # If list of binary package is not empty, we don't have to test for
            # other source name alternative.
            if pkg_list:
                return pkg_list

        raise RuntimeError(f'Could not find {srcnames} in {repo_url}, {dist}')
