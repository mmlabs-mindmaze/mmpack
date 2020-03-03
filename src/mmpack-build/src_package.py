# @mindmaze_header@
"""
Class to handle source packages, build them and generates binary packages.
"""

import os
import re
import shutil
import sys

from os import path
from subprocess import Popen
from threading import Thread
from tempfile import mkdtemp
from typing import Dict

from . workspace import Workspace, get_local_install_dir
from . binary_package import BinaryPackage
from . common import *
from . file_utils import *
from . hooks_loader import MMPACK_BUILD_HOOKS, init_mmpack_build_hooks
from . mm_version import Version
from . package_info import PackageInfo, pkginfo_get_create
from . settings import PKGDATADIR
from . syspkg_manager import get_syspkg_mgr
from . mmpack_builddep import process_dependencies, general_specs_builddeps


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


class SrcPackage:
    # pylint: disable=too-many-instance-attributes
    """
    Source package class.
    """

    def __init__(self, specfile: str, tag: str, srctar_path: str):
        # pylint: disable=too-many-arguments
        self.name = ''
        self.tag = tag
        self.version = Version(None)
        self.url = ''
        self.maintainer = ''
        self.ghost = False
        self.files_sysdep = None  # Useful only for ghost packages
        self.src_tarball = srctar_path
        self.src_hash = sha256sum(srctar_path)
        self.licenses = []
        self.copyright = ''

        self.description = ''
        self.pkg_tags = ['MindMaze']

        self.build_options = None
        self.build_system = None
        self.build_depends = []

        # dict of (name, BinaryPackage) generated from the source package
        self._packages = {}
        self.install_files_set = set()
        self._metadata_files_list = []

        dprint('loading specfile: ' + specfile)
        # keep raw dictionary version of the specfile
        self._specs = yaml_load(specfile)
        self._spec_dir = path.dirname(specfile)

        # Init source package from unpacked dir
        self._parse_specfile_general()

        if not self.licenses:
            self._default_license()

        arch_dist = get_host_arch_dist()
        init_mmpack_build_hooks(self.name, arch_dist, self.description)

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
        installdir = get_local_install_dir(self.pkgbuild_path())
        if withprefix and not self.ghost:
            installdir += _get_install_prefix()

        os.makedirs(installdir, exist_ok=True)
        return installdir

    def _guess_build_system(self):
        """
        helper: guesses the project build system

        Raises:
            RuntimeError: could not guess project build system
        """
        pushdir(self.unpack_path())
        if path.exists('configure.ac'):
            self.build_system = 'autotools'
        elif path.exists('CMakeLists.txt'):
            self.build_system = 'cmake'
        elif path.exists('Makefile'):
            self.build_system = 'makefile'
        elif path.exists('setup.py'):
            self.build_system = 'python'
        elif path.exists('meson.build'):
            self.build_system = 'meson'
        else:
            raise RuntimeError('could not guess project build system')

        popdir()

    def _remove_ignored_files(self):
        if 'ignore' in self._specs['general']:
            for regex in self._specs['general']['ignore']:
                _ = extract_matching_set(regex, self.install_files_set)
        # remove files from default ignored patterns
        _ = extract_matching_set(r'.*\.la$', self.install_files_set)
        _ = extract_matching_set(r'.*\.def$', self.install_files_set)
        _ = extract_matching_set(r'.*/__pycache__/.*', self.install_files_set)
        _ = extract_matching_set(r'.*\.pyc$', self.install_files_set)

    def _parse_specfile_general(self) -> None:
        """
        Parses the mmpack/specs file's general section.
        This will:
            - fill all the main fields of the source package.
            - prune the ignore files from the install_files_list
        """
        for key, value in self._specs['general'].items():
            if key == 'name':
                self.name = value
            elif key == 'version':
                self.version = Version(value)
            elif key == 'maintainer':
                self.maintainer = value
            elif key == 'url':
                self.url = value
            elif key == 'description':
                self.description = value
            elif key == 'build-options':
                self.build_options = value
            elif key == 'build-depends':
                self.build_depends = value
            elif key == 'build-system':
                self.build_system = value
            elif key == 'licenses':
                self.licenses = value
            elif key == 'copyright':
                self.copyright = value
            elif key == 'ghost':
                self.ghost = str2bool(value)

    def _default_license(self) -> None:
        if self.ghost:
            return

        license_file = find_license(self.unpack_path())
        if not license_file:
            errmsg = '"licenses" field unspecified and no license file found'
            raise ValueError(errmsg)
        else:
            self.licenses = [license_file]
            wrnmsg = 'Using file: "{}" as license by default' \
                     .format(license_file)
            dprint(wrnmsg)

    def _fetch_unpack_syspkg_locally(self):
        """
        Download system packages matching name and unpack them in local install
        path.
        """
        unpackdir = self._local_install_path()
        builddir = self.unpack_path() + '/build'
        os.makedirs(builddir)

        # Get syspkg source name (maybe remapped)
        srcname_remap = self._specs['general'].get('syspkg-srcnames', {})
        dist_srcname = self.name
        dist = get_host_dist()
        for regex, srcname in srcname_remap.items():
            if re.fullmatch(regex, dist):
                dist_srcname = srcname

        # Download and unpack system packages depending on target platform
        syspkg_mgr = get_syspkg_mgr()
        version, sysdeps = syspkg_mgr.fetch_unpack(dist_srcname,
                                                   builddir, unpackdir)
        self.version = version
        self.files_sysdep = sysdeps

    def _build_env(self, skip_tests: bool):
        wrk = Workspace()

        build_env = os.environ.copy()
        build_env['SRCDIR'] = self.unpack_path()
        build_env['BUILDDIR'] = mkdtemp(dir=self.unpack_path(),
                                        prefix='build-')
        build_env['DESTDIR'] = self._local_install_path()
        build_env['PREFIX'] = _get_install_prefix()
        build_env['SKIP_TESTS'] = str(skip_tests)
        build_env['PKG_CONFIG_PATH'] = path.join(_get_install_prefix(),
                                                 'lib/pkgconfig')
        if self.build_options:
            build_env['OPTS'] = self.build_options

        # enrich the env with the necessary variables to build using
        # the headers and libraries of the prefix
        if wrk.prefix:
            tmp = os.environ.get('LDFLAGS', '')
            build_env['LDFLAGS'] = '-Wl,-rpath-link={}/lib '.format(wrk.prefix)
            build_env['LDFLAGS'] += tmp

        if get_host_dist() == 'windows':
            build_env['MSYSTEM'] = 'MINGW64'
            for var in ('SRCDIR', 'BUILDDIR', 'DESTDIR', 'PREFIX'):
                build_env[var] = shell(['cygpath', '-u', build_env[var]],
                                       log=False).strip()
            for var in ('ACLOCAL_PATH',):
                build_env[var] = shell(['cygpath', '-up', build_env[var]],
                                       log=False).strip()
        return build_env

    def install_builddeps(self, prefix: str, assumeyes: bool):
        """
        install mmpack build-deps within given prefix

        !!! Requires a prefix already set up !!!
        """
        wrk = Workspace()
        cmd = '{} --prefix={} update'.format(wrk.mmpack_bin(), prefix)
        shell(cmd)

        # append platform-specific mmpack packages
        # eg. one required package is not available on this platform
        general = self._specs['general']
        system_builddeps, mmpack_builddeps = general_specs_builddeps(general)
        process_dependencies(system_builddeps, mmpack_builddeps,
                             prefix, assumeyes)

    def _build_project(self, skip_tests: bool) -> None:
        """
        Compile the unpacked sources
        guesses build system if none given.

        Raises:
            NotImplementedError: the specified build system is not supported
        """
        wrk = Workspace()

        pushdir(self.unpack_path())
        os.makedirs(self._local_install_path(), exist_ok=True)

        if not self.build_system:
            self._guess_build_system()
        if not self.build_system:
            errmsg = 'Unknown build system: ' + self.build_system
            raise NotImplementedError(errmsg)

        # Use build script provided in mmpack installed data folder unless
        # it is custom build system. The script is then obtained from
        # mmpack folder in the unpacked sources.
        build_script = '{0}/build-{1}'\
                       .format(convert_path_native(PKGDATADIR),
                               self.build_system)
        if self.build_system == 'custom':
            build_script = 'mmpack/build'

        build_cmd = ['sh', build_script]
        if wrk.prefix:
            run_prefix = [wrk.mmpack_bin(), '--prefix='+wrk.prefix, 'run']
            build_cmd = run_prefix + build_cmd

        dprint('[shell] {0}'.format(' '.join(build_cmd)))

        # Execute command and transfer output to log
        proc = Popen(build_cmd, env=self._build_env(skip_tests),
                     stdout=PIPE, stderr=PIPE, universal_newlines=True)
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
            errmsg += 'See {}/mmpack.log file for what went wrong\n' \
                      .format(self.pkgbuild_path())
            raise RuntimeError(errmsg)

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

        pushdir(self._local_install_path(True))
        if not self.ghost:
            for hook in MMPACK_BUILD_HOOKS:
                hook.post_local_install()
        self.install_files_set = set(list_files('.', exclude_dirs=True))
        popdir()

    def _ventilate_custom_packages(self) -> Dict[str, PackageInfo]:
        """
        Ventilates files explicit in the specfile before giving them to the
        default target.
        """
        dist = get_host_dist()
        pkgs = {}

        # create skeleton for explicit packages
        for pkgname, pkgspecs in self._specs.items():
            if pkgname == 'general':
                continue

            pkg = PackageInfo(pkgname)
            pkg.init_from_specs(pkgspecs, dist, self.install_files_set)

            # check that at least on file is present in the custom package.
            # Raise an error if the described package was expecting one.
            # Note: meta-packages are empty and accepted
            if not pkg.files and pkgspecs.get('files'):
                errmsg = 'Custom package {0} is empty !'.format(pkgname)
                raise FileNotFoundError(errmsg)

            pkgs[pkgname] = pkg

        return pkgs

    def _ventilate_pkg_create(self, pkgs: Dict[str, PackageInfo]):
        """
        first ventilation pass (after custom packages): check amongst the
        remaining files if one of them would trigger the creation of a new
        package.  Eg. a dynamic library will be given its own binary package
        """
        for hook in MMPACK_BUILD_HOOKS:
            hook_dispatch_data = hook.get_dispatch(self.install_files_set)
            for pkgname, files in hook_dispatch_data.items():
                pkg = pkginfo_get_create(pkgname, pkgs)
                pkg.files.update(files)
                self.install_files_set.difference_update(files)

                pkg.description = self.description + '\n'
                pkg.description += 'automatically generated around SONAME '
                pkg.description += self.name

    def _get_fallback_pkginfo(self,
                              pkgs: Dict[str, PackageInfo]) -> PackageInfo:
        """
        if a binary package is already created, use it
        if there is no binary package yet, try to fallback with a library pkg
        finally, create and fallback a binary package
        """
        if self.name in pkgs:
            return pkgs[self.name]

        libpkg = None
        for pkgname in pkgs:
            if (pkgname.startswith('lib')
                    and not (pkgname.endswith('-devel')
                             or pkgname.endswith('-doc')
                             or pkgname.endswith('-debug'))):
                if libpkg:
                    # only works if there's only one candidate
                    libpkg = None
                    break
                libpkg = pkgname
        if libpkg:
            dprint('Return default package: ' + libpkg)
            return pkginfo_get_create(libpkg, pkgs)
        dprint('Return default package: ' + self.name)
        return pkginfo_get_create(self.name, pkgs)

    def _attach_copyright(self, binpkg: BinaryPackage):
        """
        Attach the copyright and all the license files to given binary package
        Both are meant to be attached to all the binary packages.
        """
        if self.ghost:
            return

        licenses_path = set()
        if not self.licenses:
            raise RuntimeError('FATAL: license key is mandatory')

        for entry in self.licenses:
            tmp = os.path.join(PKGDATADIR, 'common-licenses', entry)
            if os.path.isfile(tmp):
                # TODO: for known licenses, create a dangling symlink to
                # mmpack common-licenses instead of copying the full file
                shutil.copy(tmp, os.path.join(binpkg.licenses_dir(), entry))
                tmp = os.path.join(binpkg.licenses_dir(), entry)
            else:
                tmp = os.path.join(self.unpack_path(), entry)
                if not os.path.isfile(tmp):
                    errmsg = 'No such file, or unknown license: ' + entry
                    raise ValueError(errmsg)
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
            with open(copyright_file, 'w') as outfile:
                outfile.write(self.copyright)

        # add a copy of the copyright to each package
        binpkg.install_files.add(copyright_file)

    def _create_binpkgs_from_pkginfos(self, pkgs: Dict[str, PackageInfo]):
        host_arch = get_host_arch_dist()
        for pkg in pkgs.values():
            pkg.version = self.version
            pkg.ghost = self.ghost
            binpkg = BinaryPackage(pkginfo=pkg,
                                   source=self.name,
                                   arch=host_arch,
                                   tag=self.tag,
                                   spec_dir=self._spec_dir,
                                   src_hash=self.src_hash)
            self._attach_copyright(binpkg)
            self._packages[pkg.name] = binpkg

            # format package description
            if binpkg.name in (self.name, self.name + '-devel',
                               self.name + '-doc', self.name + '-debug'):
                binpkg.description = self.description
                if pkg.description:
                    binpkg.description += '\n' + pkg.description
            elif not binpkg.description:
                raise ValueError('Source package {0} has no description'
                                 .format(pkg.name))

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

        pkgs = self._ventilate_custom_packages()
        self._ventilate_pkg_create(pkgs)

        tmpset = set()
        for filename in self.install_files_set:
            if is_binary(filename) or is_exec_manpage(filename):
                pkg = pkginfo_get_create(bin_pkg_name, pkgs)
            elif is_documentation(filename) or is_doc_manpage(filename):
                pkg = pkginfo_get_create(doc_pkg_name, pkgs)
            elif is_devel(filename):
                pkg = pkginfo_get_create(devel_pkg_name, pkgs)
            elif is_debugsym(filename):
                pkg = pkginfo_get_create(debug_pkg_name, pkgs)
            else:
                # skip this. It will be put in a default fallback
                # package at the end of the ventilation process
                continue

            pkg.files.add(filename)
            tmpset.add(filename)

        self.install_files_set.difference_update(tmpset)

        # deal with the remaining files:
        if self.install_files_set and not self.ghost:
            pkg = self._get_fallback_pkginfo(pkgs)
            pkg.files.update(self.install_files_set)

        self._create_binpkgs_from_pkginfos(pkgs)
        popdir()  # local-install dir

    def _generate_manifest(self) -> str:
        """
        Generate the manifest and return its path
        """

        # Generate the manifest data for binary packages
        pkgs = dict()
        for pkgname, binpkg in self._packages.items():
            pkgs[pkgname] = {'file': path.basename(binpkg.pkg_path),
                             'size': path.getsize(binpkg.pkg_path),
                             'sha256': sha256sum(binpkg.pkg_path)}

        # Generate the whole manifest data
        arch = get_host_arch_dist()
        data = {'name': self.name,
                'version': self.version,
                'binpkgs': {arch: pkgs},
                'source': {'file': path.basename(self.src_tarball),
                           'size': path.getsize(self.src_tarball),
                           'sha256': self.src_hash}}

        manifest_path = '{}_{}_{}.mmpack-manifest'.format(self.name,
                                                          self.version,
                                                          arch)
        yaml_serialize(data, manifest_path, use_block_style=True)
        return manifest_path

    def generate_binary_packages(self):
        """
        create all the binary packages
        """
        instdir = self._local_install_path(True)
        pushdir(instdir)

        wrk = Workspace()

        # Copy source package
        shutil.copy(self.src_tarball, wrk.packages)
        iprint('source {} copied in {}'
               .format(path.basename(self.src_tarball), wrk.packages))

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
                dprint('Files associated to package {}:'.format(binpkg.name))
                for filename in sorted(binpkg.install_files):
                    sysdep = self.files_sysdep[filename]
                    binpkg.add_sysdepend(self.files_sysdep[filename])
                    dprint('    {}  \t[{}]'.format(filename, sysdep))

                # Remove install files from ghost package to avoid to copy them
                # in the binary ghost package
                binpkg.install_files = set()

        for pkgname, binpkg in self._packages.items():
            pkgfile = binpkg.create(instdir, self.pkgbuild_path())
            shutil.copy(pkgfile, wrk.packages)
            iprint('generated package: {} : {}'
                   .format(pkgname,
                           path.join(wrk.packages, path.basename(pkgfile))))

        manifest = self._generate_manifest()
        shutil.copy(manifest, wrk.packages)
        iprint('generated manifest: {}'
               .format(path.join(wrk.packages, path.basename(manifest))))

        popdir()  # local install path

    def __repr__(self):
        return u'{}'.format(self.__dict__)
