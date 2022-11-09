# @mindmaze_header@
"""
Class to handle source packages, build them and generates binary packages.
"""

import os
import re
import shutil
import sys
import tarfile

from copy import copy
from os import path
from pathlib import Path
from subprocess import Popen
from threading import Thread
from tempfile import mkdtemp

from .workspace import Workspace
from .binary_package import BinaryPackage
from .common import *
from .file_utils import *
from .errors import MMPackBuildError, ShellException
from .hooks_loader import MMPACK_BUILD_HOOKS, init_mmpack_build_hooks
from .mm_version import Version
from .package_info import DispatchData, PackageInfo
from .prefix import cmd_in_optional_prefix, prefix_install, run_build_script
from .syspkg_manager import get_syspkg_mgr


_PREVENTED_BUILDENV_KEYS = (
    'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH',
    'LIBRARY_PATH', 'COMPILER_PATH', 'GCC_EXEC_PREFIX',
    'PKG_CONFIG_PATH',
    'CFLAGS', 'CXXFLAGS', 'LDFLAGS',
)


_LICDIR = os.environ.get('MMPACK_BUILD_LICENSES_DIR',
                         str(Path(__file__).parent.parent / 'common-licenses'))


class _FileConsumer(Thread):
    """
    Read in a thread from file input and duplicate onto the file output and
    logger.
    """

    def __init__(self, file_in, file_out):
        super().__init__()
        self.file_in = file_in
        self.file_out = file_out

    def run(self):
        for line in self.file_in:
            if CONFIG['debug']:
                self.file_out.write(line)
            log_info(line.strip('\n\r'))


def _get_install_prefix() -> str:
    if os.name == 'nt':
        return '/m'

    return '/run/mmpack'


def _extract_mmpack_source(srctar_path: str) -> str:
    srcdir = Workspace().tmpdir()
    with tarfile.open(srctar_path, 'r:*') as tarstream:
        tarstream.extractall(path=srcdir)
    return srcdir


class SrcPackage:
    # pylint: disable=too-many-instance-attributes
    """
    Source package class.
    """

    def __init__(self, srctar: str, buildtag: str, srcdir: str = None):
        # pylint: disable=too-many-arguments
        self.name = ''
        self.tag = buildtag
        self.version = Version(None)
        self.srcversion = self.version
        self.url = ''
        self.maintainer = ''
        self.ghost = False
        self.files_sysdep = None  # Useful only for ghost packages
        self.src_tarball = ''
        self.src_hash = sha256sum(srctar)
        self.licenses = []
        self.copyright = ''

        self.description = ''

        self.build_options = None
        self.build_system = None

        # dict of (name, BinaryPackage) generated from the source package
        self._packages = {}
        self.install_files_set = set()
        self._metadata_files_list = []

        self._specs = {}
        self._spec_dir = ''

        # Extract source tarball to temporary folder if a folder with extracted
        # source is not provided
        if not srcdir:
            srcdir = _extract_mmpack_source(srctar)

        # Init source package from unpacked dir
        self._parse_specfile_general(srcdir)
        self._prepare_pkgbuilddir(srcdir, srctar)

        if not self.licenses:
            self._default_license()

        init_mmpack_build_hooks(srcname=self.name,
                                host_archdist=get_host_arch_dist(),
                                description=self.description,
                                builddir=self.pkgbuild_path())

    def _prepare_pkgbuilddir(self, tmp_srcdir: str, srctar: str):
        """
        prepare folder for building the binary packages
        """
        wrk = Workspace()

        # Init workspace folders
        wrk.clean(self.name, self.tag)
        builddir = self.pkgbuild_path()
        unpackdir = os.path.join(builddir, self.name)

        iprint(f'moving unpacked sources from {tmp_srcdir} to {unpackdir}')
        shutil.move(tmp_srcdir, unpackdir)

        # Copy package tarball in package builddir
        new_srctar = os.path.join(builddir, os.path.basename(srctar))
        shutil.copyfile(srctar, new_srctar)

        self.src_tarball = new_srctar
        self._spec_dir = unpackdir + '/mmpack'

    def pkgbuild_path(self) -> str:
        """
        Get the package build path
        """
        wrk = Workspace()
        return wrk.builddir(srcpkg=self.name, tag=self.tag)

    def unpack_path(self) -> str:
        """
        Get the local build path, ie, the place
        where the sources are unpacked and compiled
        """
        return self.pkgbuild_path() + '/' + self.name

    def _local_install_path(self, withprefix: bool = False) -> str:
        """
        internal helper: build and return the local install path
        """
        installdir = self.pkgbuild_path() + '/local-install'
        if withprefix and not self.ghost:
            installdir += _get_install_prefix()

        os.makedirs(installdir, exist_ok=True)
        return installdir

    def _installed_srcdir(self):
        prefix = _get_install_prefix()
        name = self.name
        version = self.srcversion
        hash_suffix = self.src_hash[:4]
        return f'{prefix}/src/{name}-{version}-{hash_suffix}'

    def _guess_build_system(self):
        """
        helper: guesses the project build system

        Raises:
            MMPackBuildError: could not guess project build system
        """
        pushdir(self.unpack_path())
        if path.exists('configure.ac'):
            self.build_system = 'autotools'
        elif path.exists('CMakeLists.txt'):
            self.build_system = 'cmake'
        elif path.exists('setup.py') or path.exists('pyproject.toml'):
            self.build_system = 'python'
        elif path.exists('meson.build'):
            self.build_system = 'meson'
        elif path.exists('Makefile'):
            self.build_system = 'makefile'
        else:
            raise MMPackBuildError('could not guess project build system')

        popdir()

    def _format_description(self, binpkg: PackageInfo):
        """
        Format BinaryPackage's description.
        If the package is a default target, concat the global project
        description with the additional specific one. Otherwise, only
        use the specific description.
        Raises:
            ValueError: the description is empty for custom packages
        """
        description = binpkg.description
        if binpkg.name in (self.name, self.name + '-devel',
                           self.name + '-doc', self.name + '-debug'):
            binpkg.description = self.description
            if description:
                binpkg.description += '\n' + description

    def _remove_ignored_files(self):
        for regex in self._specs.get('ignore', []):
            extract_matching_set(regex, self.install_files_set)
        # remove files from default ignored patterns
        extract_matching_set(r'.*\.la$', self.install_files_set)
        extract_matching_set(r'.*\.def$', self.install_files_set)
        extract_matching_set(r'.*/__pycache__/.*', self.install_files_set)
        extract_matching_set(r'.*\.pyc$', self.install_files_set)

        if self.ghost:
            extract_matching_set(r'.*/share/doc(-base)?/.*',
                                 self.install_files_set)
            extract_matching_set(r'.*/lib/debug/.*', self.install_files_set)

    def _parse_specfile_general(self, srcdir: str) -> None:
        """
        Parses the mmpack/specs file's general section.
        This will:
            - fill all the main fields of the source package.
            - prune the ignore files from the install_files_list
        """
        # pylint: disable=too-many-branches

        # keep raw dictionary version of the specfile
        specfile = srcdir + '/mmpack/specs'
        dprint('loading specfile: ' + specfile)
        self._specs = specs_load(specfile)

        try:
            for key, value in self._specs.items():
                if key == 'name':
                    self.name = value
                elif key == 'version':
                    self.version = Version(value)
                    self.srcversion = self.version
                elif key == 'maintainer':
                    self.maintainer = value
                elif key == 'url':
                    self.url = value
                elif key == 'description':
                    self.description = value.strip()
                elif key == 'build-options':
                    self.build_options = value
                elif key == 'build-system':
                    self.build_system = value
                elif key == 'licenses':
                    self.licenses = value
                elif key == 'copyright':
                    self.copyright = value
                elif key == 'ghost':
                    self.ghost = str2bool(value)
        except ValueError as err:
            raise MMPackBuildError(f'Invalid spec file: {err}') from err

    def _default_license(self) -> None:
        if self.ghost:
            return

        license_file = find_license(self.unpack_path())
        if not license_file:
            errmsg = '"licenses" field unspecified and no license file found'
            raise MMPackBuildError(errmsg)

        self.licenses = [license_file]
        dprint(f'Using file: "{license_file}" as license by default')

    def _fetch_unpack_syspkg_locally(self):
        """
        Download system packages matching name and unpack them in local install
        path.
        """
        unpackdir = self._local_install_path()
        builddir = self.unpack_path() + '/build'
        os.makedirs(builddir)

        # Get syspkg source name (maybe remapped)
        dist_srcname = self.name
        srcname_remap = copy(self._specs.get('syspkg-srcnames', {}))
        try:
            dist_srcname = srcname_remap.pop('default')
        except KeyError:
            pass
        dist = get_host_dist()
        for regex, srcname in srcname_remap.items():
            if re.fullmatch(regex, dist):
                dist_srcname = srcname
        srcnames = list(map(lambda s: s.strip(), dist_srcname.split(',')))

        # Download and unpack system packages depending on target platform
        syspkg_mgr = get_syspkg_mgr()
        version, sysdeps = syspkg_mgr.fetch_unpack(srcnames,
                                                   builddir, unpackdir)
        self.version = version
        self.files_sysdep = sysdeps

    def _build_env(self, skip_tests: bool):
        build_env = os.environ.copy()

        # Prevents the build environment variable specified in build host to
        # influence package build
        for key in _PREVENTED_BUILDENV_KEYS:
            build_env.pop(key, None)

        build_env['SRCNAME'] = self.name
        build_env['SRCVERSION'] = str(self.srcversion)
        build_env['SRCDIR'] = self.unpack_path()
        build_env['BUILDDIR'] = mkdtemp(dir=self.unpack_path(),
                                        prefix='build-')
        build_env['DESTDIR'] = self._local_install_path()
        build_env['PREFIX'] = _get_install_prefix()
        build_env['INSTALLED_SRCDIR'] = self._installed_srcdir()
        build_env['SKIP_TESTS'] = str(skip_tests)
        if self.build_options:
            build_env['OPTS'] = self.build_options

        if get_host_dist() == 'windows':
            build_env['MSYSTEM'] = 'MINGW64'
            for var in ('SRCDIR', 'BUILDDIR', 'DESTDIR', 'PREFIX'):
                build_env[var] = shell(['cygpath', '-u', build_env[var]],
                                       log=False).strip()
            for var in ('ACLOCAL_PATH',):
                build_env[var] = shell(['cygpath', '-up', build_env[var]],
                                       log=False).strip()
        return build_env

    def install_builddeps(self):
        """
        install mmpack build-deps within given prefix

        !!! Requires a prefix already set up !!!
        """
        builddeps = self._specs.get('build-depends', [])
        prefix_install(builddeps)

    def _build_project(self, skip_tests: bool) -> None:
        """
        Compile the unpacked sources
        guesses build system if none given.

        Raises:
            NotImplementedError: the specified build system is not supported
        """
        pushdir(self.unpack_path())
        os.makedirs(self._local_install_path(), exist_ok=True)

        if not self.build_system:
            self._guess_build_system()

        # Use build script provided in mmpack installed data folder unless
        # it is custom build system. The script is then obtained from
        # mmpack folder in the unpacked sources.
        scriptdir = os.path.join(os.path.dirname(__file__), 'buildscripts')
        build_script = f'{scriptdir}/{self.build_system}'
        if self.build_system == 'custom':
            build_script = 'mmpack/build'

        build_cmd = cmd_in_optional_prefix(['sh', build_script])
        dprint('[shell] ' + ' '.join(build_cmd))

        # Execute command and transfer output to log
        with Popen(build_cmd, env=self._build_env(skip_tests),
                   stdout=PIPE, stderr=PIPE, universal_newlines=True) as proc:
            out = _FileConsumer(file_in=proc.stdout, file_out=sys.stdout)
            err = _FileConsumer(file_in=proc.stderr, file_out=sys.stderr)
            out.start()
            err.start()
            out.join()
            err.join()

            # Wait the command is actually finished (or failed) and inspect
            # return code
            proc.wait()
            if proc.returncode != 0:
                errmsg = 'Failed to build ' + self.name + '\n'
                errmsg += f'See {self.pkgbuild_path()}/mmpack.log file for '\
                          'what went wrong\n'
                raise ShellException(errmsg)

        popdir()  # unpack directory

    def local_install(self, skip_tests: bool = False) -> None:
        """
        local installation of the package from the source package

        fills private var: _install_files_set before returning
        """
        if self.ghost:
            self._fetch_unpack_syspkg_locally()
        else:
            self._build_project(skip_tests)

        instdir = self._local_install_path(True)
        run_build_script('local_install',
                         execdir=instdir,
                         specdir=self._spec_dir,
                         env={'SPECDIR': path.abspath(self._spec_dir)})

        pushdir(instdir)
        if not self.ghost:
            for hook in MMPACK_BUILD_HOOKS:
                hook.post_local_install()
        self.install_files_set = set(list_files('.', exclude_dirs=True))
        popdir()

    def _ventilate_custom_packages(self, data: DispatchData):
        """
        Ventilates files explicit in the specfile before giving them to the
        default target.
        """
        dist = get_host_dist()

        # create skeleton for explicit packages
        for pkgname, pkgspecs in self._specs.get('custom-pkgs', {}).items():
            pkg = data.assign_to_pkg(pkgname)
            pkg.init_from_specs(pkgspecs, dist, data.unassigned_files)
            self._format_description(pkg)

            # check that at least one file is present in the custom package.
            # Raise an error if the described package was expecting one.
            # Note: meta-packages are empty and accepted
            if not pkg.files and 'files' in pkgspecs:
                raise MMPackBuildError(f'Custom package {pkgname} is empty !')

    def _get_fallback_pkgname(self, pkg_names: Set[str]) -> str:
        """
        if a binary package is already created, use it
        if there is no binary package yet, try to fallback with a library pkg
        """
        if self.name in pkg_names:
            return self.name

        fallback_name = self.name
        libpkg = None
        for pkgname in pkg_names:
            if (pkgname.startswith('lib')
                    and not (pkgname.endswith('-devel')
                             or pkgname.endswith('-doc')
                             or pkgname.endswith('-debug'))):
                if libpkg:
                    # only works if there's only one candidate
                    fallback_name = self.name
                    break
                libpkg = pkgname
                fallback_name = libpkg

        dprint('Return default package: ' + fallback_name)
        return fallback_name

    def _attach_copyright(self, binpkg: BinaryPackage):
        """
        Attach the copyright and all the license files to given binary package
        Both are meant to be attached to all the binary packages.
        """
        if self.ghost:
            return

        licenses_path = set()
        if not self.licenses:
            raise MMPackBuildError('License could not be guessed... '
                                   'It then must be supplied in specs')

        for entry in self.licenses:
            tmp = os.path.join(_LICDIR, entry)
            if os.path.isfile(tmp):
                # TODO: for known licenses, create a dangling symlink to
                # mmpack common-licenses instead of copying the full file
                shutil.copy(tmp, os.path.join(binpkg.licenses_dir(), entry))
                tmp = os.path.join(binpkg.licenses_dir(), entry)
            else:
                tmp = os.path.join(self.unpack_path(), entry)
                if not os.path.isfile(tmp):
                    errmsg = 'No such file, or unknown license: ' + entry
                    raise MMPackBuildError(errmsg)
                shutil.copy(tmp, binpkg.licenses_dir())

            licenses_path.add(os.path.join(binpkg.licenses_dir(),
                                           os.path.basename(tmp)))
        binpkg.install_files.update(licenses_path)

        if not self.copyright:
            return

        # dump copyright to dedicated file in install tree if needed
        if os.path.isfile(self.copyright):
            shutil.copy(self.copyright, binpkg.licenses_dir())
            copyright_file = self.copyright
        else:
            copyright_file = binpkg.licenses_dir() + '/copyright'
            with open(copyright_file, 'w', encoding='utf-8') as outfile:
                outfile.write(self.copyright)

        # add a copy of the copyright to each package
        binpkg.install_files.add(copyright_file)

    def _create_binpkgs_from_dispatch(self, data: DispatchData):
        host_arch = get_host_arch_dist()
        for pkginfo in data.pkgs.values():
            binpkg = BinaryPackage(name=pkginfo.name,
                                   version=self.version,
                                   source=self.name,
                                   arch=host_arch,
                                   tag=self.tag,
                                   spec_dir=self._spec_dir,
                                   src_hash=self.src_hash,
                                   ghost=self.ghost)
            binpkg.install_files = pkginfo.files

            # Init dependency and system dependency that were already specified
            # from specs of custom packages
            for dep, minver, maxver in pkginfo.deplist:
                binpkg.add_depend(dep, minver, maxver)
            for sysdep in pkginfo.sysdeps:
                binpkg.add_sysdepend(sysdep)

            binpkg.description = pkginfo.description
            if not binpkg.description:
                raise MMPackBuildError(f'Pkg {binpkg.name} has no description')

            self._attach_copyright(binpkg)
            self._packages[binpkg.name] = binpkg
            dprint('created package ' + binpkg.name)

    def ventilate(self):
        """
        Ventilate files.

        Must be called after local-install, otherwise it will return dummy
        packages with no files.

        Naming:
          For source package 'xxx1' named after project 'xxx' of version 1.0.0
          create packages xxx1, xxx1-devel, xxx1-doc

          There is no conflict between the source and the binary package names
          because the packages types are different.
        """
        pushdir(self._local_install_path(True))

        bin_pkg_name = self.name
        doc_pkg_name = self.name + '-doc'
        devel_pkg_name = self.name + '-devel'
        debug_pkg_name = self.name + '-debug'

        self._remove_ignored_files()
        if not self.install_files_set:
            # warn when no binary package will be created
            # do not abort here however: this may be ok when
            # working with virtual packages
            iprint('No installed files found! No package will be created.')

        # Start dispatching files in package starting from custom packages
        # specified in specs and continuing with the result of dispatch hooks
        data = DispatchData(self.install_files_set)
        self._ventilate_custom_packages(data)
        for hook in MMPACK_BUILD_HOOKS:
            hook.dispatch(data)

        for filename in data.unassigned_files.copy():
            if is_binary(filename) or is_exec_manpage(filename):
                pkgname = bin_pkg_name
            elif is_documentation(filename) or is_doc_manpage(filename):
                pkgname = doc_pkg_name
            elif is_devel(filename):
                pkgname = devel_pkg_name
            elif is_debugsym(filename):
                pkgname = debug_pkg_name
            else:
                # skip this. It will be put in a default fallback
                # package at the end of the ventilation process
                continue

            pkg = data.assign_to_pkg(pkgname, {filename})
            if not pkg.description:
                self._format_description(pkg)

        # deal with the remaining files:
        if data.unassigned_files and not self.ghost:
            pkgname = self._get_fallback_pkgname(data.pkgs.keys())
            pkg = data.assign_to_pkg(pkgname, data.unassigned_files)
            if not pkg.description:
                self._format_description(pkg)

        self._create_binpkgs_from_dispatch(data)
        popdir()  # local-install dir

    def _generate_manifest(self) -> str:
        """
        Generate the manifest and return its path
        """
        # Generate the manifest data for binary packages
        pkgs = {
            pkgname: {
                'file': path.basename(binpkg.pkg_path),
                'size': path.getsize(binpkg.pkg_path),
                'sha256': sha256sum(binpkg.pkg_path)
            }
            for pkgname, binpkg in self._packages.items()
        }

        # Generate the whole manifest data
        arch = get_host_arch_dist()
        data = {'name': self.name,
                'version': self.srcversion,
                'binpkgs': {arch: pkgs},
                'source': {'file': path.basename(self.src_tarball),
                           'size': path.getsize(self.src_tarball),
                           'sha256': self.src_hash}}

        outdir = self.pkgbuild_path()
        mpath = f'{outdir}/{self.name}_{self.version}_{arch}.mmpack-manifest'
        yaml_serialize(data, mpath, use_block_style=True)
        return mpath

    def generate_binary_packages(self):
        """
        create all the binary packages
        """
        instdir = self._local_install_path(True)
        pushdir(instdir)

        outdir = Workspace().outdir()

        # Copy source package
        shutil.copy(self.src_tarball, outdir)
        iprint(f'source {path.basename(self.src_tarball)} copied in {outdir}')

        # we need all of the provide infos before starting the dependencies
        for pkgname, binpkg in self._packages.items():
            binpkg.gen_provides()

        for pkgname, binpkg in self._packages.items():
            binpkg.gen_dependencies(self._packages.values())

        # for ghost packages, the files are provided by the system package
        # manager instead of the mmpack packages. They are only needed for
        # generating dependencies and seeing what is provided.
        if self.ghost:
            for binpkg in self._packages.values():

                # Assign actual system package dependency of ghost package
                dprint(f'Files associated to package {binpkg.name}:')
                for filename in sorted(binpkg.install_files):
                    sysdep = self.files_sysdep.get(filename)
                    # local_install_script are allowed to remove and add files
                    # to local-install. It is thus possible to encounter a file
                    # not provided by any syspkg.
                    if not sysdep:
                        continue
                    binpkg.add_sysdepend(sysdep)
                    dprint(f'    {filename}  \t[{sysdep}]')

                # Remove install files from ghost package to avoid to copy them
                # in the binary ghost package
                binpkg.install_files = set()

        for pkgname, binpkg in self._packages.items():
            pkgfile = binpkg.create(instdir, self.pkgbuild_path())
            shutil.copy(pkgfile, outdir)
            pkgpath = path.join(outdir, path.basename(pkgfile))
            iprint(f'generated package: {pkgname} : {pkgpath}')

        manifest = self._generate_manifest()
        shutil.copy(manifest, outdir)
        manifest_path = path.join(outdir, path.basename(manifest))
        iprint(f'generated manifest: {manifest_path}')

        popdir()  # local install path

    def build_binpkgs(self, skip_tests: bool = False):
        """Build and create binary packages."""
        set_log_file(self.pkgbuild_path() + '/mmpack.log')

        self.local_install(skip_tests)
        self.ventilate()
        self.generate_binary_packages()

    def __repr__(self):
        return repr(self.__dict__)
