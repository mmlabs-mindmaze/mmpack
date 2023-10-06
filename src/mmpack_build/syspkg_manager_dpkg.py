# @mindmaze_header@
"""
helper module containing dpkg files parsing functions
"""

import os
import re

from email import message_from_bytes, message_from_string
from functools import partial
from glob import glob
from importlib import import_module
from typing import List, TextIO, Iterable, Iterator, NamedTuple, Optional

from .common import *
from .errors import MMPackBuildError, ShellException
from .mm_version import Version
from .syspkg_manager_base import SysPkgManager, SysPkg
from .workspace import cached_download


DPKG_METADATA_PREFIX = '/var/lib/dpkg/info'


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
    shlib_soname_regex = re.compile(rf'\b{name} {version}\b .*')

    guess = glob(DPKG_METADATA_PREFIX + '/'
                 + guess_pkgname + '**.shlibs')
    if guess and os.path.exists(guess[0]):
        return guess[0]

    for shlibs_file in glob(DPKG_METADATA_PREFIX + '/**.shlibs'):
        with open(shlibs_file, encoding='utf-8') as stream:
            if shlib_soname_regex.search(stream.read()):
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
    shlib_soname = f'{name} {version} '
    with open(filename, encoding='utf-8') as stream:
        for line in stream:
            if line.startswith(shlib_soname):
                dependency_template = line[len(shlib_soname):-1]
                break
    if not dependency_template:
        raise Assert(target_soname + 'not found in ' + filename)

    # read library file to prune symbols
    # assert the existence of <pkgname>*.list
    pkgname = dependency_template.split(' ')[0]
    pattern = f'{DPKG_METADATA_PREFIX}/{pkgname}**:{get_host_arch()}.list'
    dpkg_list_file = glob(pattern)[0]
    with open(dpkg_list_file, encoding='utf-8') as listfile:
        for line in (ln.strip('\n') for ln in listfile if target_soname in ln):
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
    arch = get_host_arch()

    # make sure the library characters are not interpreted as PCRE
    # Eg: libstdc++.so.6
    #            ^^^  ^  all those are PCRE wildcards
    # Note: '\b' is PCRE for word boundary
    symbols_soname_regex = re.compile(rf'\b{re.escape(target_soname)}\b .*')

    guess = glob(f'{DPKG_METADATA_PREFIX}/{guess_pkgname}**:{arch}.symbols')
    if guess and os.path.exists(guess[0]):
        return guess[0]

    for symbols_file in glob(f'{DPKG_METADATA_PREFIX}/**:{arch}.symbols'):
        with open(symbols_file, encoding='utf-8') as stream:
            if symbols_soname_regex.search(stream.read()):
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

    with open(filename, encoding='utf-8') as symbol_file:
        for line in symbol_file:
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
            minver = f'(>= {str(minversion)})'
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
    pattern = f'/usr/lib/python3/dist-packages/{pypkg}[/|.py|.*.so]*'
    try:
        cmd_output = shell(['dpkg', '--search', pattern])
        debpkg_list = list({r.split(':')[0] for r in cmd_output.splitlines()})
        return debpkg_list[0] if debpkg_list else None
    except ShellException:
        return None


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
        'Description': 'desc',
    }

    def __init__(self, pkg_str: str):
        super().__init__()

        msg = message_from_string(pkg_str)
        self.name = msg['package']
        self.version = msg['Version']
        self.source = msg['Source']
        self.filename = msg['Filename']
        self.sha256 = msg['SHA256']
        self.desc = msg['Description']
        self.desc_md5 = msg['Description-md5']

        if not self.source:
            self.source = self.name

        if ' ' in self.source:
            self.source = self.source.split(' ', 1)[0]

    def get_sysdep(self) -> str:
        return f'{self.name} (>= {self.version})'


def _list_debpkg_index(fileobj: TextIO) -> Iterator[DebPkg]:
    pkg_str = ''
    for line in iter(partial(fileobj.readline), ''):
        if line != '\n' or not pkg_str:
            pkg_str += line
            continue

        yield pkg_str
        pkg_str = ''


class _FileInfo(NamedTuple):
    size: str
    sha: str
    filename: str


class DebRepo:
    """
    Wrapper of Debian repository
    """
    def __init__(self, url: str, distcomp: str, builddir: str,
                 arch: Optional[str] = None):
        dist, _, comp = distcomp.partition('/')

        release_url = f'{url}/dists/{dist}/Release'
        release = message_from_bytes(get_http_req(release_url).data)

        # Check component is valid for this repo
        if comp not in release['Components'].split():
            raise MMPackBuildError(f'No {comp} in {release_url}')

        self._base_url = url
        self._builddir = builddir
        self._dist = dist
        self._comp = comp

        sha_section = release['SHA256'].strip().split('\n')
        self._shalist = {c[2]: _FileInfo(filename=c[2], sha=c[0], size=c[1])
                         for c in [f.strip().split() for f in sha_section]}

        self._arch = arch or get_host_arch()
        if self._arch not in set(release['Architectures'].split()):
            raise MMPackBuildError(f'No arch {self._arch} in {url} {distcomp}')

    def _fetch_distfile(self, res: str) -> str:
        for ext in ('.gz', '.xz', '.bz2', ''):
            comp_res = f'{self._comp}/{res}{ext}'
            if comp_res in self._shalist:
                comp_url = f'{self._base_url}/dists/{self._dist}/{comp_res}'
                filename = os.path.join(self._builddir,
                                        comp_res.replace('/', '_'))
                cached_download(comp_url, filename,
                                expected_sha256=self._shalist[comp_res].sha)
                return filename

        raise MMPackBuildError(f'cannot find {res} in '
                               f'{self._base_url} {self._comp}')

    def translate_pkgs(self, pkgs: Iterable[DebPkg]):
        """Replace short description of package with long description."""
        md5_pkg = {p.desc_md5: p for p in pkgs if p.desc_md5 is not None}

        # Download translation, but missing one is not an error
        try:
            filename = self._fetch_distfile('i18n/Translation-en')
        except MMPackBuildError:
            return

        # Read translation and try to load description matching package
        with open_compressed_file(filename, encoding='utf-8') as fileobj:
            for entry in _list_debpkg_index(fileobj):
                msg = message_from_string(entry)
                md5 = msg['Description-md5']
                if md5 in md5_pkg:
                    body = msg['Description-en'].partition('\n ')[2]
                    body = body.replace('\n .\n ', '\n')
                    body = body.replace('\n ', ' ')
                    md5_pkg[md5].desc = body

    def pkgs(self) -> Iterator[DebPkg]:
        """
        Iterator of package in the distribution
        """
        index = self._fetch_distfile(f'binary-{self._arch}/Packages')
        with open_compressed_file(index, encoding='utf-8') as fileobj:
            for entry in _list_debpkg_index(fileobj):
                yield DebPkg(entry)


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
        return (base_url, distcomp)

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

    def parse_pkgindex(self, builddir: str,
                       srcnames: List[str]) -> List[SysPkg]:
        # Try to get the repo that provide the specified source package
        try:
            repo_url, distcomp = _get_repo(srcnames)
        except ValueError:
            return []

        repo = DebRepo(repo_url, distcomp, builddir)
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
                repo.translate_pkgs(pkg_list)
                return pkg_list

        raise MMPackBuildError(f'Could not find {srcnames} in'
                               ' {repo_url}, {dist}')
