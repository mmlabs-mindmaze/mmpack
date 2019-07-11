# @mindmaze_header@
'''
Class to handle source packages, build them and generates binary packages.
'''

import importlib
import os
from glob import glob
import re
import shutil
from subprocess import Popen
from tempfile import mkdtemp
import tarfile
from threading import Thread
from typing import Set
import sys
from workspace import Workspace, get_local_install_dir
from binary_package import BinaryPackage
from common import *
from file_utils import *
from mm_version import Version
from settings import PKGDATADIR
from mmpack_builddep import process_dependencies, general_specs_builddeps


class _FileConsumer(Thread):
    ''' Read in a thread from file input and duplicate onto the file output
        and logger.
    '''

    def __init__(self, file_in, file_out):
        super().__init__()
        self.file_in = file_in
        self.file_out = file_out

    def run(self):
        for line in self.file_in:
            self.file_out.write(line)
            log_info(line.strip('\n\r'))


def _get_install_prefix() -> str:
    if os.name == 'nt':
        return '/m'

    return '/run/mmpack'


def _unpack_deps_version(item):
    ''' helper to allow simpler mmpack dependency syntax

    expected full mmpack dependency syntax is:
        <name>: [min_version, max_version]

    this allows the additional with implicit 'any' as maximum:
        <name>: min_version
        <name>: any
    '''
    try:
        name, minv, maxv = item
        return (name, Version(minv), Version(maxv))
    except ValueError:
        name, minv = item  # if that fails, let the exception be raised
        minv = Version(minv)
        maxv = Version('any')
        return (name, minv, maxv)


def create_source_from_git(url: str, tag: str = None):
    ''' Create a source package from git clone

    Returns: An initialized source package
    '''

    wrk = Workspace()

    git_opts = '--quiet --depth=1'
    if tag:
        git_opts += ' --branch ' + tag

    iprint('cloning ' + url)
    clonedir = mkdtemp(dir=wrk.sources)
    shell('git clone {0} {1} {2}'.format(git_opts, url, clonedir))

    pushdir(clonedir)

    # Get tag name if not set yet (use current branch)
    if not tag:
        tag = shell('git rev-parse --abbrev-ref HEAD').strip()

    # Get package name and version
    specs = yaml_load('mmpack/specs')
    name = specs['general']['name']
    version = specs['general']['version']

    wrk.clean(name, tag)

    srcpkg = SrcPackage(name, tag)
    builddir = srcpkg.pkgbuild_path()
    unpackdir = srcpkg.unpack_path()

    # Create source package tarball
    src_tarball = '{0}/{1}_{2}_src.tar.gz'.format(builddir, name, version)
    cmd = 'git archive --format=tar.gz {} > {}'.format(tag, src_tarball)
    shell(cmd)
    shutil.copy(src_tarball, wrk.packages)

    popdir()  # clone dir

    iprint('moving cloned files from {0} to {1}'.format(clonedir, builddir))
    shutil.move(clonedir, unpackdir)

    srcpkg.load_specfile()
    srcpkg.parse_specfile()

    return srcpkg


def load_source_from_tar(tarpath: str, tag: str = None):
    ''' Load mmpack source package from a tarball.

    Returns: An initialized source package
    '''

    wrk = Workspace()

    if not tag:
        tag = 'from_tar'

    tmpdir = mkdtemp(dir=wrk.sources)

    iprint('extracting temporarily to ' + tmpdir)
    tar = tarfile.open(tarpath, 'r:*')
    tar.extractall(path=tmpdir)

    # Get package name and version
    specs = yaml_load(tmpdir + '/mmpack/specs')
    name = specs['general']['name']
    version = specs['general']['version']

    wrk.clean(name, tag)

    srcpkg = SrcPackage(name, tag)
    builddir = srcpkg.pkgbuild_path()
    unpackdir = srcpkg.unpack_path()

    # Create source package tarball (ensure that source are properly
    # renamed)
    src_tarball = '{0}/{1}_{2}_src.tar.gz'.format(builddir, name, version)
    shutil.copy(tarpath, src_tarball)
    shutil.copy(src_tarball, wrk.packages)

    iprint('moving unpacked files from {0} to {1}'.format(tmpdir, builddir))
    shutil.move(tmpdir, unpackdir)

    srcpkg.load_specfile()
    srcpkg.parse_specfile()

    return srcpkg


class SrcPackage:
    # pylint: disable=too-many-instance-attributes
    '''
    Source package class.
    '''

    def __init__(self, name: str, tag: str = None):
        # pylint: disable=too-many-arguments
        self.name = name
        self.tag = tag
        self.version = None
        self.url = None
        self.maintainer = None

        self.description = ''
        self.pkg_tags = ['MindMaze']

        self.build_options = None
        self.build_system = None
        self.build_depends = []

        self._specs = None  # raw dictionary version of the specfile
        # dict of (name, BinaryPackage) generated from the source package
        self._packages = {}
        self.install_files_set = set()
        self._metadata_files_list = []

    def pkgbuild_path(self) -> str:
        'Get the package build path'
        wrk = Workspace()
        return wrk.builddir(srcpkg=self.name, tag=self.tag)

    def unpack_path(self) -> str:
        ''' Get the local build path, ie, the place
        where the sources are unpacked and compiled
        '''
        return self.pkgbuild_path() + '/' + self.name

    def _local_install_path(self, withprefix: bool = False) -> str:
        'internal helper: build and return the local install path'
        installdir = get_local_install_dir(self.pkgbuild_path())
        if withprefix:
            installdir += _get_install_prefix()

        os.makedirs(installdir, exist_ok=True)
        return installdir

    def _guess_build_system(self):
        ''' helper: guesses the project build system

        Raises:
            RuntimeError: could not guess project build system
        '''
        pushdir(self.unpack_path())
        if os.path.exists('configure.ac'):
            self.build_system = 'autotools'
        elif os.path.exists('CMakeLists.txt'):
            self.build_system = 'cmake'
        elif os.path.exists('Makefile'):
            self.build_system = 'makefile'
        elif os.path.exists('setup.py'):
            self.build_system = 'python'
        elif os.path.exists('meson.build'):
            self.build_system = 'meson'
        else:
            raise RuntimeError('could not guess project build system')

        popdir()

    def load_specfile(self, specfile: str = None) -> None:
        ''' Load the specfile and store it as its dictionary equivalent
        '''
        if not specfile:
            specfile = self.unpack_path() + '/mmpack/specs'
        dprint('loading specfile: ' + specfile)
        self._specs = yaml_load(specfile)

    def _get_matching_files(self, pcre: str) -> Set[str]:
        ''' given some pcre, return the set of matching files from
            self.install_files_set.
            Those files are removed from the source install file set.
        '''
        matching_set = set()
        for inst_file in self.install_files_set:
            if re.match(pcre, inst_file):
                matching_set.add(inst_file)

        self.install_files_set.difference_update(matching_set)
        return matching_set

    def _format_description(self, binpkg: BinaryPackage, pkgname: str,
                            pkg_type: str = None):
        ''' Format BinaryPackage's description.

        If the package is a default target, concat the global project
        description with the additional spcific one. Otherwise, only
        use the specific description.

        Raises: ValueError if the description is empty for custom packages
        '''
        try:
            description = self._specs[pkgname]['description']
        except KeyError:
            description = None

        if binpkg.name in (self.name, self.name + '-devel',
                           self.name + '-doc', self.name + '-debug'):
            binpkg.description = self.description
            if description:
                binpkg.description += '\n' + description
        else:
            if not description and pkg_type == 'custom':
                raise ValueError('Source package {0} has no description'
                                 .format(pkgname))
            elif not description and pkg_type == 'library':
                description = self.description + '\n'
                description += 'automatically generated around SONAME '
                description += self.name
            binpkg.description = description

    def _remove_ignored_files(self):
        if 'ignore' in self._specs['general']:
            for regex in self._specs['general']['ignore']:
                _ = self._get_matching_files(regex)
        # remove *.la and *.def files
        _ = self._get_matching_files(r'.*\.la$')
        _ = self._get_matching_files(r'.*\.def$')

    def _parse_specfile_general(self) -> None:
        ''' Parses the mmpack/specs file's general section.
        This will:
            - fill all the main fields of the source package.
            - prune the ignore files from the install_files_list
        '''
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

    def _binpkg_get_create(self, binpkg_name: str,
                           pkg_type: str = None) -> BinaryPackage:
        'Returns the newly create BinaryPackage if any'
        if binpkg_name not in self._packages:
            host_arch = get_host_arch_dist()
            binpkg = BinaryPackage(binpkg_name, self.version,
                                   self.name, host_arch, self.tag)
            self._format_description(binpkg, binpkg_name, pkg_type)
            self._packages[binpkg_name] = binpkg
            dprint('created package ' + binpkg_name)
            return binpkg
        return self._packages[binpkg_name]

    def parse_specfile(self) -> None:
        '''
            - create BinaryPackage skeleton entries foreach custom and default
              entries.
        '''
        self._parse_specfile_general()

        host_arch = get_host_arch_dist()
        sysdeps_key = 'sysdepends-' + get_host_dist()

        # create skeleton for explicit packages
        for pkg in self._specs.keys():
            if pkg != 'general':
                binpkg = BinaryPackage(pkg, self.version, self.name,
                                       host_arch, self.tag)
                self._format_description(binpkg, pkg)

                if 'depends' in self._specs[pkg]:
                    for dep in self._specs[pkg]['depends']:
                        item = list(dep.items())[0]
                        name, minv, maxv = _unpack_deps_version(item)
                        binpkg.add_depend(name, minv, maxv)
                if sysdeps_key in self._specs[pkg]:
                    for dep in self._specs[pkg][sysdeps_key]:
                        binpkg.add_sysdepend(dep)
                self._packages[pkg] = binpkg

    def _build_env(self, skip_tests: bool):
        wrk = Workspace()

        build_env = os.environ.copy()
        build_env['SRCDIR'] = self.unpack_path()
        build_env['BUILDDIR'] = self.unpack_path() + '/build'
        build_env['DESTDIR'] = self._local_install_path()
        build_env['PREFIX'] = _get_install_prefix()
        build_env['LD_RUN_PATH'] = os.path.join(_get_install_prefix(), 'lib')
        build_env['SKIP_TESTS'] = str(skip_tests)
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

    def _strip_dirs_from_install_files(self):
        tmp = {x for x in self.install_files_set if not os.path.isdir(x)}
        self.install_files_set = tmp

    def install_builddeps(self, prefix: str, assumeyes: bool):
        ''' install mmpack build-deps within given prefix

        !!! Requires a prefix already set up !!!
        '''
        wrk = Workspace()
        cmd = '{} --prefix={} update'.format(wrk.mmpack_bin(), prefix)
        shell(cmd)

        # append platform-specific mmpack packages
        # eg. one required package is not available on this platform
        general = self._specs['general']
        system_builddeps, mmpack_builddeps = general_specs_builddeps(general)
        process_dependencies(system_builddeps, mmpack_builddeps,
                             prefix, assumeyes)

    def local_install(self, skip_tests: bool = False) -> None:
        ''' local installation of the package from the source package

        guesses build system if none given.
        fills private var: _install_list before returning

        Returns:
            the list of all the installed files
        Raises:
            NotImplementedError: the specified build system is not supported
        '''
        wrk = Workspace()

        pushdir(self.unpack_path())
        os.makedirs('build')
        os.makedirs(self._local_install_path(), exist_ok=True)

        if not self.build_system:
            self._guess_build_system()
        if not self.build_system:
            errmsg = 'Unknown build system: ' + self.build_system
            raise NotImplementedError(errmsg)

        # Use build script provided in mmpack installed data folder unless
        # it is custom build system. The script is then obtained from
        # mmpack folder in the unpacked sources.
        build_script = '{0}{1}/build-{2}'\
                       .format(wrk.cygroot(), PKGDATADIR, self.build_system)
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
            errmsg += 'See build.log file for what went wrong\n'
            raise RuntimeError(errmsg)

        popdir()  # unpack directory

        pushdir(self._local_install_path(True))
        self.install_files_set = set(glob('**', recursive=True))
        self._strip_dirs_from_install_files()
        popdir()

    def _ventilate_custom_packages(self):
        ''' Ventilates files explicit in the specfile before
            giving them to the default target.
        '''
        for binpkg in self._packages:
            if 'files' in self._specs[binpkg]:
                for regex in self._specs[binpkg]['files']:
                    matching_set = self._get_matching_files(regex)
                    self._packages[binpkg].install_files.update(matching_set)

        # check that at least on file is present in each of the custom packages
        # raise an error if the described package was expecting one
        # Note: meta-packages are empty and accepted
        for binpkg in self._packages:
            if not self._packages[binpkg] and self._packages[binpkg]['files']:
                errmsg = 'Custom package {0} is empty !'.format(binpkg)
                raise FileNotFoundError(errmsg)

    def _ventilate_pkg_create(self):
        ''' first ventilation pass (after custom packages):
            check amongst the remaining files if one of them would trigger
            the creation of a new package.
            Eg. a dynamic library will be given its own binary package
        '''
        ventilated = set()
        for file in self.install_files_set:
            libtype = is_dynamic_library(file)
            if libtype in ('elf', 'pe'):
                format_module = importlib.import_module(libtype + '_utils')
                soname = format_module.soname(file)
                name, version = parse_soname(soname)
                binpkgname = name + version  # libxxx.0.1.2 -> libxxx<ABI>
                pkg = self._binpkg_get_create(binpkgname, 'library')
                pkg.install_files.add(file)
                ventilated.add(file)

                # add the soname file to the same package
                so_filename = os.path.dirname(file) + '/' + soname
                pkg.install_files.add(so_filename)
                ventilated.add(so_filename)

        self.install_files_set.difference_update(ventilated)

    def _get_fallback_package(self, bin_pkg_name: str) -> BinaryPackage:
        '''
        if a binary package is already created, use it
        if there is no binary package yet, try to fallback with a library pkg
        finally, create and fallback a binary package
        '''

        if bin_pkg_name in self._packages:
            return self._packages[bin_pkg_name]

        libpkg = None
        for pkgname in self._packages:
            if (pkgname.startswith('lib')
                    and not (pkgname.endswith('-devel')
                             or pkgname.endswith('-doc')
                             or pkgname.endswith('-debug'))):
                if libpkg:
                    # works if there's only one candidate
                    return None
                libpkg = pkgname
        if libpkg:
            dprint('Return default package: ' + libpkg)
            return self._binpkg_get_create(libpkg)
        dprint('Return default package: ' + bin_pkg_name)
        return self._binpkg_get_create(bin_pkg_name)

    def ventilate(self):
        ''' Ventilate files.
        Must be called after local-install, otherwise it will return dummy
        packages with no files.

        Naming:
          For source package 'xxx1' named after project 'xxx' of version 1.0.0
          create packages xxx1, xxx1-devel, xxx1-doc

          There is no conflict between the source and the binary package names
          because the packages types are different.
        '''
        pushdir(self._local_install_path(True))

        bin_pkg_name = self.name
        doc_pkg_name = self.name + '-doc'
        devel_pkg_name = self.name + '-devel'
        debug_pkg_name = self.name + '-debug'

        self._remove_ignored_files()
        self._ventilate_custom_packages()
        self._ventilate_pkg_create()

        if not self.install_files_set:
            # warn when no binary package will be created
            # do not abort here however: this may be ok when
            # working with virtual packages
            iprint('No installed files found! No package will be created.')

        tmpset = set()
        for filename in self.install_files_set:
            if is_binary(filename) or is_exec_manpage(filename):
                pkg = self._binpkg_get_create(bin_pkg_name)
            elif is_documentation(filename) or is_doc_manpage(filename):
                pkg = self._binpkg_get_create(doc_pkg_name)
            elif is_devel(filename):
                pkg = self._binpkg_get_create(devel_pkg_name)
            elif is_debugsym(filename):
                pkg = self._binpkg_get_create(debug_pkg_name)
            else:
                # skip this. It will be put in a default fallback
                # package at the end of the ventilation process
                continue

            pkg.install_files.add(filename)
            tmpset.add(filename)

        self.install_files_set.difference_update(tmpset)

        # deal with the remaining files:
        if self.install_files_set:
            pkg = self._get_fallback_package(bin_pkg_name)
            pkg.install_files.update(self.install_files_set)

        popdir()

    def generate_binary_packages(self):
        'create all the binary packages'
        instdir = self._local_install_path(True)
        pushdir(instdir)

        wrk = Workspace()

        # Computer hash of source
        srctar = '{}/{}_{}_src.tar.gz'.format(self.pkgbuild_path(),
                                              self.name, self.version)
        src_hash = sha256sum(srctar)
        shutil.copy(srctar, wrk.packages)
        iprint('source {} copied in {}'.format(os.path.basename(srctar),
                                               wrk.packages))

        # we need all of the provide infos before starting the dependencies
        for pkgname, binpkg in self._packages.items():
            binpkg.gen_provides()

        for pkgname, binpkg in self._packages.items():
            binpkg.gen_dependencies(self._packages.values())
            binpkg.src_hash = src_hash
            pkgfile = binpkg.create(instdir, self.pkgbuild_path())
            shutil.copy(pkgfile, wrk.packages)
            iprint('generated package: {}'.format(pkgname))
        popdir()  # local install path

    def __repr__(self):
        return u'{}'.format(self.__dict__)
