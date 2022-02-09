# @mindmaze_header@
"""
plugin tracking containing python file handling functions
"""

import filecmp
import os
import re
from email.parser import Parser
from glob import glob, iglob
from os.path import basename
from typing import Set, Dict, List, Iterable, NamedTuple

from . base_hook import BaseHook
from . common import shell, Assert, iprint, rmtree_force
from . file_utils import filetype
from . package_info import PackageInfo, DispatchData
from . provide import Provide, ProvideList, load_mmpack_provides
from . syspkg_manager import get_syspkg_mgr
from . workspace import Workspace


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


def _gen_pysymbols(pkgfiles: Set[str], sitedirs: List[str]) -> Set[str]:
    # Filter out files that are not python script nor subpath of any sitedirs
    sites_files = set()
    for sitedir in sitedirs:
        sites_files.update({f for f in pkgfiles
                            if _is_py_file(f)
                            and os.path.commonpath([f, sitedir]) == sitedir})

    # Skip processing if there are no python package in sitedirs
    if not sites_files:
        return set()

    script = os.path.join(os.path.dirname(__file__), 'python_provides.py')
    cmd = ['python3', script]
    cmd += ['--site-path='+path for path in sitedirs]
    cmd += list(sites_files)

    cmd_output = shell(cmd)
    return set(cmd_output.split())


def _gen_py_provides(pkg: PackageInfo, sitedirs: List[str]) -> ProvideList:
    providelist = ProvideList('python')
    symbols = _gen_pysymbols(pkg.files, sitedirs)

    # Group provided symbols by python package names (import name)
    for pyname in {s.split('.', maxsplit=1)[0] for s in symbols}:
        pyname_syms = {s for s in symbols if s.startswith(pyname + '.')}
        provide = Provide(pyname)
        provide.pkgdepends = pkg.name
        provide.add_symbols(pyname_syms, pkg.version)
        providelist.add(provide)

    return providelist


def _gen_pydepends(pkg: PackageInfo, sitedirs: List[str]) -> Set[str]:
    script = os.path.join(os.path.dirname(__file__), 'python_depends.py')
    cmd = ['python3', script]
    cmd += ['--site-path='+path for path in sitedirs]
    cmd += [f for f in pkg.files if _is_py_file(f)]

    # run in prefix if one is being used, this allows to establish dependency
    # against installed mmpack packages
    wrk = Workspace()
    if wrk.prefix:
        cmd = [wrk.mmpack_bin(), '--prefix='+wrk.prefix, 'run'] + cmd

    cmd_output = shell(cmd)
    return set(cmd_output.split())


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
    def __init__(self):
        self.files = set()
        self.name = None
        self.import_names = set()
        self.single_egginfo = False
        self.meta_top = set()

    def add(self, filename: str, info: PyNameInfo):
        """
        Register an installed file in python package
        """
        if not info.is_metadata:
            self.import_names.update({info.pyname})

        if not self.name and (filename.endswith('.egg-info/PKG-INFO')
                              or filename.endswith('.dist-info/METADATA')
                              or filename.endswith('.egg-info')):
            self.name = _parse_metadata(filename)['Name']

        self.single_egginfo = filename.endswith('.egg-info') and not self.files

        if info.is_metadata and filename.endswith('/top_level.txt'):
            dirs = {d.strip() for d in open(filename).readlines()}
            self.meta_top.update(dirs)

        self.files.add(filename)

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
        return 'python3-' + name.replace('_', '-')


def _assign_privname_to_pypkg(priv_name, pypkgs: Dict[str, _PyPkg]) -> _PyPkg:
    # Pick package that has same name as private name excepting for leading '_'
    pypkg = pypkgs.get(priv_name[1:])
    if pypkg:
        return pypkg

    # Pick the package that has metadata whose topdir list private name
    for pypkg in {p for p in pypkgs.values() if p.meta_top}:
        if priv_name in pypkg.meta_top:
            return pypkg

    # Pick the first that is not a private package
    return list({p for n, p in pypkg.items() if not n.startswith('_')})[0]


def _assign_metadata(metapkg, pypkgs: Dict[str, _PyPkg]) -> _PyPkg:
    public_pkgs = {n: p for n, p in pypkgs.items() if p.import_names}

    pypkg = public_pkgs.get(metapkg.name)
    if pypkg:
        return pypkg

    for pyname, pypkg in public_pkgs.items():
        if pyname.startswith(metapkg.name):
            return pypkg

    return list(public_pkgs.values())[0]


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

    def _gen_py_deps(self, currpkg: PackageInfo, used_symbols: Set[str],
                     others_pkgs: List[PackageInfo]):
        """
        For each key (imported package name) in `imports` determine the mmpack
        or system dependency that provides it and add it to those of
        `currpkg`.

        Args:
            currpkg: package whose dependencies are computed (and added)
            used_symbols: py symbols used in currpkg
            others_pkgs: list of packages cobuilded (may include currpkg)
        """
        imports = {s.split('.', maxsplit=1)[0] for s in used_symbols}

        # provided in the same package or a package being generated
        for pkg in others_pkgs:
            dep_list = pkg.provides['python'].gen_deps(imports, used_symbols)
            dep_list += pkg.provides['pypriv'].gen_deps(imports, used_symbols)
            for pkgname, _ in dep_list:
                currpkg.add_to_deplist(pkgname, pkg.version, pkg.version)

        # provided by another mmpack package present in the prefix
        dep_list = self._get_mmpack_provides().gen_deps(imports, used_symbols)
        for pkgname, version in dep_list:
            currpkg.add_to_deplist(pkgname, version)

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
        pypkgs = {}
        for file in data.unassigned_files.copy():
            try:
                info = _parse_py3_filename(file)
                pypkg = pypkgs.setdefault(info.pyname.lower(), _PyPkg())
                if info.is_metadata and basename(file) in _IGNORE_METADATA:
                    data.unassigned_files.discard(file)
                else:
                    pypkg.add(file, info=info)
            except FileNotFoundError:
                pass

        # If single file egg-info, this is a metadata common for all python
        # packages. => merge all them
        for pypkg in pypkgs.values():
            if pypkg.single_egginfo:
                for otherpkg in pypkgs.values():
                    pypkg.merge_pkg(otherpkg)
                pypkgs = {pypkg.name: pypkg}
                break

        # Merge python packages with a private import name into one of the
        # other packages
        for name in {n for n in pypkgs if n.startswith('_')}:
            pypkg = _assign_privname_to_pypkg(name, pypkgs)
            pypkg.merge_pkg(pypkgs.pop(name))

        # Merge package that is only metadata to compatible one
        metapkgs = {n: p for n, p in pypkgs.items() if not p.import_names}
        for name, metapkg in metapkgs.items():
            pypkg = _assign_metadata(metapkg, pypkgs)
            pypkg.merge_pkg(pypkgs.pop(name))

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
