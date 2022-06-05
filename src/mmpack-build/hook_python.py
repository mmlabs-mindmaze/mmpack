# @mindmaze_header@
"""
plugin tracking containing python file handling functions
"""

import filecmp
import json
import os
import re
from email.parser import Parser
from glob import glob, iglob
from itertools import chain
from os.path import basename
from typing import Set, Dict, List, Iterable, NamedTuple, Optional

from .base_hook import BaseHook
from .common import shell, Assert, iprint, rmtree_force
from .file_utils import filetype
from .package_info import PackageInfo, DispatchData
from .prefix import cmd_in_optional_prefix
from .provide import Provide, ProvideList, load_mmpack_provides, pkgs_provides
from .syspkg_manager import get_syspkg_mgr
from .workspace import Workspace


# example of matches:
# 'lib/python3.6/site-packages/foo.so'
#               => (lib/python3.6/site-packages, foo, .so)
# 'lib/python3/site-packages/_foo.so'
#               => (lib/python3/site-packages, foo, .so)
# 'mingw64/lib/python3/site-packages/_foo.so'
#               => (mingw64/lib/python3/site-packages, foo, .so)
# 'usr/lib/python3/dist-packages/_foo.so'
#               => (usr/lib/python3/dist-packages, foo, .so)
# 'lib/python3/site-packages/foo_bar.py'
#               => (lib/python3/site-packages, foo_bar, .py)
# 'lib/python3/site-packages/foo/__init__.py'
#               => (lib/python3/site-packages, foo, None)
# 'lib/python3/site-packages/foo/_internal.so'
#               => (lib/python3/site-packages, foo, None)
# 'lib/python3/site-packages/Foo-1.2.3.egg-info/_internal.so'
#               => (lib/python3/site-packages, foo, -1.2.3-egg-info)
# 'lib/python2/site-packages/foo.so'
#               => None
_PKG_REGEX = re.compile(
    r'((?:usr/|mingw64/)?lib/python3(?:\.\d)?/(?:dist|site)-packages)'
    r'/([\w-]+)([^/]*)'
)

_PYEXT_REGEX = re.compile(r'.*\.cpython-.*\.(?:so|dll|pyd)')
_IGNORE_METADATA = [
    'direct_url.json',
    'LICENSE',
    'INSTALLER',
    'RECORD',
    'REQUESTED',
    'WHEEL',
]


# location relative to the prefix dir where the files of public python packages
# must be installed
_MMPACK_REL_PY_SITEDIR = 'lib/python3/site-packages'


class PyNameInfo(NamedTuple):
    """
    information about a file regarding import name, sitedir and whether the
    file contains metadata.
    """
    pyname: str
    sitedir: str
    is_metadata: bool


def _parse_metadata(filename: str) -> dict:
    return dict(Parser().parse(open(filename, encoding='utf-8')))


def _parse_py3_filename(filename: str) -> PyNameInfo:
    """
    Get the python3 package name a file should belongs to (ie the one with
    __init__.py file if folder, or the name of the single module if directly in
    site-packages folder) along with the site dir that the package belongs to.

    Args:
        filename: file to test

    Return: PyNameInfo data about `filename`

    Raises:
        FileNotFoundError: the file does not belong to a public python package
    """
    res = _PKG_REGEX.search(filename)
    if not res:
        raise FileNotFoundError

    grps = res.groups()

    sitedir = grps[0]
    name = grps[1]
    ext = grps[2]

    is_metadata = (ext.endswith('.egg-info')
                   or ext.endswith('.dist-info')
                   or ext.endswith('-nspkg.pth'))
    return PyNameInfo(pyname=name, sitedir=sitedir, is_metadata=is_metadata)


def _is_py_file(filename: str) -> bool:
    ftype = filetype(filename)
    if ftype in ('py', 'python', 'python3'):
        return True

    return bool(_PYEXT_REGEX.fullmatch(filename))


def _gen_py_importname(pyfiles: Iterable[str],
                       sitedirs: List[str]) -> Dict[str, Set[str]]:
    cmdfiles = [f for f in pyfiles if _is_py_file(f)]
    if not cmdfiles:
        return {}

    script = os.path.join(os.path.dirname(__file__), 'python_dispatch.py')
    cmd = ['python3', script]
    cmd += ['--site-path='+path for path in sitedirs]
    cmd += cmdfiles

    cmd_output = shell(cmd)
    return {k: set(v) for k, v in json.loads(cmd_output).items()}


def _gen_pysymbols(pkgfiles: Set[str],
                   sitedirs: List[str]) -> Dict[str, Set[str]]:
    # Filter out files that are not python script nor subpath of any sitedirs
    sites_files = set()
    for sitedir in sitedirs:
        sites_files.update({f for f in pkgfiles
                            if _is_py_file(f)
                            and os.path.commonpath([f, sitedir]) == sitedir})

    # Skip processing if there are no python package in sitedirs
    if not sites_files:
        return {}

    script = os.path.join(os.path.dirname(__file__), 'python_provides.py')
    cmd = ['python3', script]
    cmd += ['--site-path='+path for path in sitedirs]
    cmd += list(sites_files)

    cmd_output = shell(cmd)
    return {k: set(v) for k, v in json.loads(cmd_output).items()}


def _gen_py_provides(pkg: PackageInfo, sitedirs: List[str]) -> ProvideList:
    providelist = ProvideList('python')
    symbols = _gen_pysymbols(pkg.files, sitedirs)

    # Group provided symbols by python package names (import name)
    for pyname, pyname_syms in symbols.items():
        provide = Provide(pyname)
        provide.pkgdepends = pkg.name
        provide.add_symbols(pyname_syms, pkg.version)
        providelist.add(provide)

    return providelist


def _resolve_ns_deps(provides: ProvideList, pkg: PackageInfo,
                     modname: str, symbols: Set[str]) -> bool:
    while '.' in modname:
        # Get parent package module name
        modname = modname.rsplit('.', maxsplit=1)[0]

        # Try to resolve dependencies with only the parent package as import
        imports = {modname}
        provides.resolve_deps(pkg, imports, symbols)

        # If imports is empty, the parent import has been found
        if not imports:
            return True

    return False


def _gen_pydepends(pkg: PackageInfo,
                   sitedirs: List[str]) -> Dict[str, Set[str]]:
    script = os.path.join(os.path.dirname(__file__), 'python_depends.py')
    cmd = ['python3', script]
    cmd += ['--site-path='+path for path in sitedirs]
    cmd += [f for f in pkg.files if _is_py_file(f)]

    # run in prefix if one is being used, this allows to establish dependency
    # against installed mmpack packages
    cmd_output = shell(cmd_in_optional_prefix(cmd))
    return {k: set(v) for k, v in json.loads(cmd_output).items()}


def _get_packaged_public_sitedirs(pkg: PackageInfo) -> Set[str]:
    if not pkg.ghost:
        return {_MMPACK_REL_PY_SITEDIR}

    # List python3 sitedir that expose public packages
    sitedirs = set()
    for pkgfile in pkg.files:
        match = _PKG_REGEX.match(pkgfile)
        if not match:
            continue
        sitedirs.add(match.groups()[0])

    return sitedirs


class _PyPkg:
    def __init__(self, import_name: Optional[str] = None,
                 files: Optional[Set[str]] = None):
        self.files = files if files else set()
        self.name = None
        self.import_names = {import_name} if import_name else set()
        self.meta_top = set()

    def add_metadata_files(self, files: Set[str]):
        """
        Register installed metadata files in python package
        """
        for filename in files:
            if not self.name and (filename.endswith('.egg-info/PKG-INFO')
                                  or filename.endswith('.dist-info/METADATA')
                                  or filename.endswith('.egg-info')):
                self.name = _parse_metadata(filename)['Name']

            if filename.endswith('/top_level.txt'):
                dirs = {d.strip() for d in open(filename).readlines()}
                self.meta_top.update(dirs)

        self.files.update(files)

    def merge_pkg(self, pypkg):
        """
        Merge registered file of 2 python package and update their metadata
        accordingly
        """
        self.files.update(pypkg.files)
        self.import_names.update(pypkg.import_names)
        self.meta_top.update(pypkg.meta_top)
        self.name = self.name if self.name else pypkg.name

    def mmpack_pkgname(self) -> str:
        """
        Return the name of the mmpack package python package
        """
        names = {n.lower() for n in self.import_names if not n.startswith('_')}
        if self.name:
            pyname = self.name.lower().replace('-', '_')
            if pyname.startswith('python_'):
                pyname = pyname[len('python_'):]

            if not names:
                names = {pyname}
            elif len(names) > 1:
                filternames = {n for n in names if n.startswith(pyname)}
                if filternames:
                    names = filternames

        # Pick one name if more than one candidate
        name = list(names)[0]
        name = name.translate({ord('_'): '-', ord('.'): '-'})
        return 'python3-' + name


def _assign_privname_to_pypkg(priv_name, pypkgs: Dict[str, _PyPkg]) -> _PyPkg:
    # Pick package that has same name as private name excepting for leading '_'
    # of last component in modpath
    modpath = priv_name.split('.')
    pypkg = pypkgs.get('.'.join(modpath[:-1] + [modpath[-1][1:]]))
    if pypkg:
        return pypkg

    # Pick the package that has metadata whose topdir list private name
    for pypkg in {p for p in pypkgs.values() if p.meta_top}:
        if priv_name in pypkg.meta_top:
            return pypkg

    # Pick the first that is not a private package
    return list({p for n, p in pypkg.items() if not n.startswith('_')})[0]


def _assign_metadata(metaname, pypkgs: Dict[str, _PyPkg]) -> _PyPkg:
    pypkg = pypkgs.get(metaname)
    if pypkg:
        return pypkg

    for pyname, pypkg in pypkgs.items():
        if pyname.startswith(metaname):
            return pypkg

    return list(pypkgs.values())[0]


#####################################################################
# Python hook for mmpack-build
#####################################################################

class MMPackBuildHook(BaseHook):
    """
    Hook tracking python module used and exposed
    """
    def __init__(self, srcname: str, host_archdist: str, description: str):
        super().__init__(srcname, host_archdist, description)
        self._mmpack_py_provides = None
        self._private_sitedirs = []

    def _get_mmpack_provides(self) -> ProvideList:
        """
        Get all shared library soname and associated symbols for all mmpack
        package installed in prefix. The parsing of all .symbols files is
        cached, hence subsequent calls to this method is very fast.
        """
        if not self._mmpack_py_provides:
            self._mmpack_py_provides = load_mmpack_provides('pyobjects',
                                                            'python')
        return self._mmpack_py_provides

    def _gen_py_deps(self, currpkg: PackageInfo, usedpkgs: Dict[str, Set[str]],
                     others_pkgs: List[PackageInfo]):
        """
        For each key (imported package name) in `imports` determine the mmpack
        or system dependency that provides it and add it to those of
        `currpkg`.

        Args:
            currpkg: package whose dependencies are computed (and added)
            used_symbols: dict of import name -> symbols used in currpkg
            others_pkgs: list of packages cobuilded (may include currpkg)
        """
        imports = set(usedpkgs.keys())
        symbols = set(chain.from_iterable(usedpkgs.values()))

        # provided in the same package or a package being generated
        for ptype in ('python', 'pypriv'):
            provides = pkgs_provides(others_pkgs, ptype)
            provides.resolve_deps(currpkg, imports, symbols, True)

        # provided by another mmpack package present in the prefix
        provides = self._get_mmpack_provides()
        provides.resolve_deps(currpkg, imports, symbols)
        for modname in [imp for imp in imports if '.' in imp]:
            if _resolve_ns_deps(provides, currpkg, modname, symbols):
                imports.remove(modname)

        # provided by the host system
        syspkg_mgr = get_syspkg_mgr()
        for pypkg in imports:
            sysdep = syspkg_mgr.find_pypkg_sysdep(pypkg)
            if not sysdep:
                # <pypkg> dependency could not be met with any available means
                errmsg = 'Could not find package providing {} python package'\
                         .format(pypkg)
                raise Assert(errmsg)

            currpkg.add_sysdep(sysdep)

    def _guess_private_sitedirs(self, pkgnames: Iterable[str]):
        for name in set(list(pkgnames) + [self._srcname]):
            # Search for python files in potential private sitedir
            sitedir = 'share/' + name
            for path in iglob(sitedir + '/**', recursive=True):
                if os.path.isfile(path) and _is_py_file(path):
                    iprint(f'{sitedir} could be a private python sitedir')
                    self._private_sitedirs.append(sitedir)
                    break

    def post_local_install(self):
        """
        install move python3 packages from versioned python3 install folder
        to a unversioned python3 folder. This way, python3 version can be
        upgraded (normally, only python3 standard library has to be
        installed in a version installed folder).
        """
        # Move all public package from unversioned python3 folder to an
        # unversioned one
        for pydir in glob('lib/python3.*/site-packages'):
            os.makedirs(_MMPACK_REL_PY_SITEDIR, exist_ok=True)
            for srcdir, dirs, files in os.walk(pydir):
                reldir = os.path.relpath(srcdir, pydir)
                dstdir = os.path.join(_MMPACK_REL_PY_SITEDIR, reldir)

                # Create the folders in unversioned python3 site package
                for dirpath in dirs:
                    os.makedirs(os.path.join(dstdir, dirpath), exist_ok=True)

                # Move files to unversioned python3 site package
                for filename in files:
                    src = os.path.join(srcdir, filename)
                    dst = os.path.join(dstdir, filename)

                    # If destination file exists, we have no issue if source
                    # and destination are the same
                    if os.path.lexists(dst):
                        if filecmp.cmp(src, dst):
                            continue
                        raise FileExistsError

                    os.replace(src, dst)

            # Remove the remainings
            rmtree_force(pydir)

        # Transform versioned egg-info to unversioned one
        egginfo_re = re.compile(r'(.*)-py3(?:\.\d\w*)*.egg-info$')
        for eggdir in glob(_MMPACK_REL_PY_SITEDIR + '/*.egg-info'):
            match = egginfo_re.fullmatch(eggdir)
            if match:
                os.rename(eggdir, match.group(1) + '.egg-info')

    def dispatch(self, data: DispatchData):
        pypkgs: Dict[str, _PyPkg] = {}
        sitedirs = set()
        pyfiles = set()
        metafiles = {}
        for file in data.unassigned_files.copy():
            try:
                info = _parse_py3_filename(file)
                if info.is_metadata:
                    if basename(file) in _IGNORE_METADATA:
                        data.unassigned_files.discard(file)
                    else:
                        metafiles.setdefault(info.pyname, set()).add(file)
                else:
                    sitedirs.add(info.sitedir)
                    pyfiles.add(file)
            except FileNotFoundError:
                pass

        # Create python packages based on python import names
        for mod, files in _gen_py_importname(pyfiles, list(sitedirs)).items():
            pypkgs[mod] = _PyPkg(import_name=mod, files=files)

        # Assign metadata to split python packages
        for name, files in metafiles.items():
            _assign_metadata(name, pypkgs).add_metadata_files(files)

        # Merge python packages with a private import name into one of the
        # other packages
        for name in {n for n in pypkgs if n.split('.')[-1].startswith('_')}:
            pypkg = _assign_privname_to_pypkg(name, pypkgs)
            pypkg.merge_pkg(pypkgs.pop(name))

        # Assign python package to dispatched package (maybe create them)
        for name, pypkg in pypkgs.items():
            pkgname = pypkg.mmpack_pkgname()
            pkg = data.assign_to_pkg(pkgname, pypkg.files)
            if pkg.description:
                continue

            pyname = pypkg.name if pypkg.name else name
            pkg.description = '{}\nThis contains the python3 package {}'\
                              .format(self._src_description, pyname)

        self._guess_private_sitedirs(data.pkgs.keys())

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        # Add public python package
        public_sitedirs = _get_packaged_public_sitedirs(pkg)
        py3_provides = _gen_py_provides(pkg, public_sitedirs)
        py3_provides.update_from_specs(specs_provides, pkg)
        pkg.provides['python'] = py3_provides

        # Register private python package for cobuilded package dependency
        # resolution
        pkg.provides['pypriv'] = _gen_py_provides(pkg, self._private_sitedirs)

    def store_provides(self, pkg: PackageInfo, folder: str):
        filename = '{}/{}.pyobjects.gz'.format(folder, pkg.name)
        pkg.provides['python'].serialize(filename)

    def update_depends(self, pkg: PackageInfo, other_pkgs: List[PackageInfo]):
        # Ignore dependency finding if ghost package
        if pkg.ghost:
            return

        py_scripts = [f for f in pkg.files if _is_py_file(f)]
        if not py_scripts:
            return

        sitedirs = [_MMPACK_REL_PY_SITEDIR] + self._private_sitedirs
        used_symbols = _gen_pydepends(pkg, sitedirs)
        self._gen_py_deps(pkg, used_symbols, other_pkgs)
