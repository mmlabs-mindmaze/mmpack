# @mindmaze_header@
'''
Class to handle source packages, build them and generates binary packages.
'''

import os
from glob import glob
import re
import tarfile
from typing import List
import yaml
from workspace import Workspace
from binary_package import BinaryPackage
from common import shell, pushdir, popdir, ShellException, \
         dprint, iprint, eprint, get_host_arch
from version import Version


# compiled regex for ventilating files
BIN_PKG_FILE_RE = re.compile(r'.*(\.1\.0\.0|/bin/.*|/locale/.*|/man/*\.1)')
DOC_PKG_FILE_RE = re.compile(r'.*(/man/.*\.[^123]|/doc/.*)')
DEVEL_PKG_FILE_RE = re.compile(r'.*(/man/.*\.(2|3)|/include/.*|\.so)')
DEBUG_PKG_FILE_RE = re.compile(r'.*\.debug')


def _guess_project_build_system() -> str:
    ''' helper: guesses the project build system

    Raises:
        RuntimeError: could not guess project build system
    '''
    if os.path.exists('configure.ac'):
        return 'autotools'
    elif os.path.exists('CMakeLists.txt'):
        return 'cmake'
    elif os.path.exists('Makefile'):
        return 'makefile'
    else:
        raise RuntimeError('could not guess project build system')


class Package(object):
    # pylint: disable=too-many-instance-attributes
    '''
    Source package class.
    '''
    def __init__(self, url: str=None, tag: str=None):
        # pylint: disable=too-many-arguments
        self.name = None
        self.tag = tag
        self.version = None
        self.url = url
        self.maintainer = None

        self.description = ''
        self.pkg_tags = ['MindMaze']

        self.build_options = None
        self.build_depends = []

        self._specs = None  # raw dictionary version of the specfile
        # dict of (name, Package) of packages generated from the source package
        self._packages = {}
        self.install_files_list = []
        self._metadata_files_list = []

    def _local_build_path(self) -> str:
        'internal helper: build and return the local build path'
        wrk = Workspace()
        return wrk.builddir(self.name) + '/{0}-{1}'.format(self.name,
                                                           self.tag)

    def load_specfile(self, specfile: str=None) -> None:
        ''' Load the specfile and store it as its dictionary equivalent
        '''
        if specfile:
            self._specs = yaml.load(open(specfile, 'rb').read())
        else:
            wrk = Workspace()
            pushdir(wrk.packages)
            self._specs = yaml.load(open('mmpack/specs', 'rb').read())
            popdir()

    def _get_matching_files(self, pcre: str) -> List[str]:
        ''' given some pcre, return the list of matching files from
            self.install_files_list.
            Those files are removed from the source install file list.
        '''
        matching_list = []
        for inst_file in self.install_files_list:
            if re.match(pcre, inst_file):
                matching_list.append(inst_file)
                self.install_files_list.remove(inst_file)

        return matching_list

    def _format_description(self, binpkg: BinaryPackage, pkgname: str):
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
            if not description:
                raise ValueError('Package {0} has no description'
                                 .format(pkgname))
            binpkg.description = description

    def _remove_ignored_files(self):
        if 'ignore' in self._specs['general']:
            for regex in self._specs['general']['ignore']:
                _ = self._get_matching_files(regex)

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
                self.version = value
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

    def _binpkg_get_create(self, binpkg_name: str) -> BinaryPackage:
        'Returns the newly create BinaryPackage if any'
        if binpkg_name not in self._packages:
            (cpu_arch, dist) = get_host_arch()
            host_arch = '{0}-{1}'.format(cpu_arch, dist)
            binpkg = BinaryPackage(binpkg_name, self.version,
                                   self.name, host_arch)
            self._format_description(binpkg, binpkg_name)
            self._packages[binpkg_name] = binpkg
            return binpkg
        return self._packages[binpkg_name]

    def parse_specfile(self) -> None:
        '''
            - create BinaryPackage skeleton entries foreach custom and default
              entries.
        '''
        self._parse_specfile_general()

        (cpu_arch, dist) = get_host_arch()
        host_arch = '{0}-{1}'.format(cpu_arch, dist)
        sysdeps_key = 'sysdepends-' + dist

        # create skeleton for explicit packages
        for pkg in self._specs.keys():
            if pkg != 'general':
                binpkg = BinaryPackage(pkg, self.version, self.name, host_arch)
                self._format_description(binpkg, pkg)

                if 'depends' in self._specs[pkg]:
                    for dep in self._specs[pkg]['depends']:
                        name, version = list(dep.items())[0]
                        binpkg.add_depend(name, Version(version))
                if sysdeps_key in self._specs[pkg]:
                    for dep in self._specs[pkg][sysdeps_key]:
                        name, version = list(dep.items())[0]
                        binpkg.add_sysdepend(name, Version(version))
                self._packages[pkg] = binpkg

    def create_source_archive(self) -> str:
        ''' Create git archive (source package).

        Returns: the name of the archive file
        '''
        wrk = Workspace()

        pushdir(wrk.sources)
        sources_archive_name = '{0}-{1}-src.tar.gz'.format(self.name, self.tag)
        iprint('cloning ' + self.url)
        cmd = 'git clone --quiet ' + self.url
        shell(cmd)
        pushdir(self.name)
        cmd = 'git archive --format=tar.gz --prefix={0}-{1}/ {1}' \
              ' > {2}/{3}'.format(self.name, self.tag,
                                  wrk.sources, sources_archive_name)
        shell(cmd)
        popdir()  # repository name

        # copy source package to package repository
        cmd = 'cp -vf {0} {1}'.format(sources_archive_name, wrk.packages)
        shell(cmd)
        popdir()  # workspace sources directory

        return sources_archive_name

    def local_install(self, source_pkg: str, build_system: str=None) -> None:
        ''' local installation of the package from the source package

        guesses build system if none given.
        fills private var: _install_list before returning

        Args:
            build_system: (optional) possible values are
                'autotools', 'cmake', 'makefile'
        Returns:
            the list of all the installed files
        Raises:
            NotImplementedError: the specified build system is not supported
        '''
        wrk = Workspace()

        pushdir(wrk.packages)
        archive = tarfile.open(source_pkg, 'r:gz')
        archive.extractall(wrk.builddir(self.name))
        archive.close()
        popdir()  # workspace packages directory

        pushdir(self._local_build_path())
        os.makedirs('build')
        os.makedirs('install')

        if not build_system:
            build_system = _guess_project_build_system()

        if build_system == 'autotools':
            cmd = '{{ cd build ' \
                  ' && ../autogen.sh && ../configure --prefix={0}/install' \
                  ' && make && make check && make install ;' \
                  ' }} 2> build.stderr'.format(os.getcwd())
        elif build_system == 'cmake':
            cmd = '{{ cd build ' \
                  ' && cmake .. -DCMAKE_INSTALL_PREFIX:PATH={0}/install' \
                  ' && make && make check && make install ;' \
                  ' }} 2> build.stderr'.format(os.getcwd())
        elif build_system == 'makefile':
            cmd = '{{ make && make check && make install PREFIX={0}/install;' \
                  ' }} 2> build.stderr'.format(os.getcwd())
        else:
            raise NotImplementedError('Unknown build system: ' + build_system)

        try:
            shell(cmd)
        except ShellException:
            eprint('Build failed. See build.stderr file for more infos.')
            raise
        popdir()  # local build directory

        pushdir(self._local_build_path() + '/install')
        self.install_files_list = glob(os.getcwd() + '/**', recursive=True)
        popdir()

    def _ventilate_custom_packages(self):
        ''' Ventilates files explicited in the specfile before
            giving them to the default target.
        '''
        for binpkg in self._packages:
            for regex in self._packages[binpkg]['files']:
                for file in self.install_files_list:
                    if re.match(regex, file):
                        self._packages[binpkg].install_files.append(file)

    def ventilate(self) -> List[BinaryPackage]:
        ''' Ventilate files.
        Must be called after local-install, otherwise it will return dummy
        packages with no files.

        Naming:
          For source package 'xxx1' named after project 'xxx' of version 1.0.0
          create packages xxx1, xxx1-devel, xxx1-doc

          There is no conflict between the source and the binary package names
          because the packages types are different.
        '''
        bin_pkg_name = self.name
        doc_pkg_name = self.name + '-doc'
        devel_pkg_name = self.name + '-devel'
        debug_pkg_name = self.name + '-debug'

        self._remove_ignored_files()
        self._ventilate_custom_packages()

        for file in self.install_files_list:
            if BIN_PKG_FILE_RE.match(file):
                pkg = self._binpkg_get_create(bin_pkg_name)
            elif DOC_PKG_FILE_RE.match(file):
                pkg = self._binpkg_get_create(doc_pkg_name)
            elif DEVEL_PKG_FILE_RE.match(file):
                pkg = self._binpkg_get_create(devel_pkg_name)
            elif DEBUG_PKG_FILE_RE.match(file):
                pkg = self._binpkg_get_create(debug_pkg_name)
            else:
                dprint('ventilate: unknown file category - {0}'.format(file))
                pkg = self._binpkg_get_create(bin_pkg_name)

            pkg.install_files.append(file)

        return self._packages

    def __repr__(self):
        return u'{}'.format(self.__dict__)
