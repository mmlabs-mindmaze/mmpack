# @mindmaze_header@
'''
Class to handle source packages, build them and generates binary packages.
'''

import os
from glob import glob
import re
from subprocess import PIPE, run
import tarfile
from typing import List
import yaml
from workspace import Workspace
from binary_package import BinaryPackage
from common import shell, pushdir, popdir, dprint, iprint, get_host_arch, \
         filetype
from elf_utils import elf_soname
from version import Version
from settings import PKGDATADIR


# compiled regex for ventilating files
BIN_PKG_FILE_RE = re.compile(r'.*(/bin/.*|/man/*\.1)')
DOC_PKG_FILE_RE = re.compile(r'.*(/man/.*\.[^123]|/doc/.*)')
DEVEL_PKG_FILE_RE = re.compile(r'.*(/man/.*\.(2|3)|/include/.*|\.so)')
DEBUG_PKG_FILE_RE = re.compile(r'.*\.debug')


def _get_install_prefix() -> str:
    if os.name == 'nt':
        return '/m/'
    else:  # if not windows, then linux
        return '/run/mmpack/'


def _is_dynamic_library(filename: str) -> str:
    'Return filetype if format conforms to lib*.so* '
    basename = os.path.basename(filename)
    if ('/lib/' in filename
            and basename.startswith('lib')
            and '.so' in basename):
        return filetype(filename)


class SrcPackage(object):
    # pylint: disable=too-many-instance-attributes
    '''
    Source package class.
    '''
    def __init__(self, srcname: str, url: str=None, tag: str=None):
        # pylint: disable=too-many-arguments
        self.srcname = srcname
        self.name = None
        self.tag = tag
        self.version = None
        self.url = url
        self.maintainer = None

        self.description = ''
        self.pkg_tags = ['MindMaze']

        self.build_options = None
        self.build_system = None
        self.build_depends = []

        self._specs = None  # raw dictionary version of the specfile
        # dict of (name, BinaryPackage) generated from the source package
        self._packages = {}
        self.install_files_list = []
        self._metadata_files_list = []

    def _local_build_path(self) -> str:
        'internal helper: build and return the local build path'
        wrk = Workspace()
        return wrk.builddir(self.name) + '/{0}-{1}'.format(self.name, self.tag)

    def _local_install_path(self, withprefix: bool=False) -> str:
        'internal helper: build and return the local install path'
        installdir = self._local_build_path() + '/install'
        if withprefix:
            installdir += _get_install_prefix()

        os.makedirs(installdir, exist_ok=True)
        return installdir

    def _guess_build_system(self):
        ''' helper: guesses the project build system

        Raises:
            RuntimeError: could not guess project build system
        '''
        pushdir(self._local_build_path())
        if os.path.exists('configure.ac'):
            self.build_system = 'autotools'
        elif os.path.exists('CMakeLists.txt'):
            self.build_system = 'cmake'
        elif os.path.exists('Makefile'):
            self.build_system = 'makefile'
        else:
            raise RuntimeError('could not guess project build system')

        popdir()

    def load_specfile(self, specfile: str=None) -> None:
        ''' Load the specfile and store it as its dictionary equivalent
        '''
        wrk = Workspace()
        if not specfile:
            specfile = '{0}/{1}/mmpack/specs'.format(wrk.sources,
                                                     self.srcname)
        dprint('loading specfile: ' + specfile)
        self._specs = yaml.load(open(specfile, 'rb').read())

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

    def _format_description(self, binpkg: BinaryPackage, pkgname: str,
                            pkg_type: str=None):
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
                binpkg.description = self.description + '\n'
                binpkg.description += 'automatically generated around SONAME '
                binpkg.description += self.name
            binpkg.description = description

    def _remove_ignored_files(self):
        if 'ignore' in self._specs['general']:
            for regex in self._specs['general']['ignore']:
                _ = self._get_matching_files(regex)
        # remove *.la files
        _ = self._get_matching_files(r'.*.la')

    def _parse_specfile_general(self) -> None:
        ''' Parses the mmpack/specs file's general section.
        This will:
            - fill all the main fields of the source package.
            - prune the ignore files from the install_files_list
        '''
        for key, value in self._specs['general'].items():
            dprint('[specs] setting {0}={1}'.format(key, value))
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
                           pkg_type: str=None) -> BinaryPackage:
        'Returns the newly create BinaryPackage if any'
        if binpkg_name not in self._packages:
            (cpu_arch, dist) = get_host_arch()
            host_arch = '{0}-{1}'.format(cpu_arch, dist)
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

        (cpu_arch, dist) = get_host_arch()
        host_arch = '{0}-{1}'.format(cpu_arch, dist)
        sysdeps_key = 'sysdepends-' + dist

        # create skeleton for explicit packages
        for pkg in self._specs.keys():
            if pkg != 'general':
                binpkg = BinaryPackage(pkg, self.version, self.name,
                                       host_arch, self.tag)
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
        wrk.srcclean(self.srcname)
        pushdir(wrk.sources)
        sources_archive_name = '{0}-{1}-src.tar.gz' \
                               .format(self.srcname, self.tag)
        iprint('cloning ' + self.url)
        cmd = 'git clone --quiet {0} {1}'.format(self.url, self.srcname)
        shell(cmd)
        pushdir(self.srcname)
        cmd = 'git archive --format=tar.gz --prefix={0}/ {1}' \
              ' > {2}/{3}'.format(self.srcname, self.tag,
                                  wrk.sources, sources_archive_name)
        shell(cmd)
        popdir()  # repository name

        # copy source package to package repository
        cmd = 'cp -vf {0} {1}'.format(sources_archive_name, wrk.packages)
        shell(cmd)
        popdir()  # workspace sources directory

        return sources_archive_name

    def _build_env(self):
        build_env = os.environ.copy()
        build_env['SRCDIR'] = self._local_build_path()
        build_env['BUILDDIR'] = self._local_build_path() + '/build'
        build_env['DESTDIR'] = self._local_install_path()
        build_env['PREFIX'] = _get_install_prefix()
        if self.build_options:
            build_env['OPTS'] = self.build_options
        return build_env

    def _strip_dirs_from_install_files(self):
        tmp = [x for x in self.install_files_list if not os.path.isdir(x)]
        self.install_files_list = tmp

    def local_install(self, source_pkg: str) -> None:
        ''' local installation of the package from the source package

        guesses build system if none given.
        fills private var: _install_list before returning

        Returns:
            the list of all the installed files
        Raises:
            NotImplementedError: the specified build system is not supported
        '''
        wrk = Workspace()
        wrk.clean(self.name)

        pushdir(wrk.packages)
        archive = tarfile.open(source_pkg, 'r:gz')
        archive.extractall(wrk.builddir(self.name))
        archive.close()
        popdir()  # workspace packages directory

        pushdir(self._local_build_path())
        os.makedirs('build')
        os.makedirs(self._local_install_path(), exist_ok=True)

        if not self.build_system:
            self._guess_build_system()
        if not self.build_system:
            errmsg = 'Unknown build system: ' + self.build_system
            raise NotImplementedError(errmsg)

        build_script = ['/bin/sh',
                        '{0}/build-{1}'.format(PKGDATADIR, self.build_system)]
        dprint('[shell] {0}'.format(' '.join(build_script)))
        ret = run(build_script, stdout=PIPE, env=self._build_env())
        if ret.returncode != 0:
            errmsg = 'Failed to build ' + self.name + '\n'
            errmsg += 'Stderr:\n'
            errmsg += ret.stderr.decode('utf-8')
            raise RuntimeError(errmsg)

        popdir()  # local build directory

        pushdir(self._local_install_path(True))
        self.install_files_list = glob('./**', recursive=True)
        self._strip_dirs_from_install_files()
        popdir()

    def _ventilate_custom_packages(self):
        ''' Ventilates files explicited in the specfile before
            giving them to the default target.
        '''
        ventilated = []
        for binpkg in self._packages:
            for regex in self._packages[binpkg]['files']:
                for file in self.install_files_list:
                    if re.match(regex, file):
                        self._packages[binpkg].install_files.append(file)
                        ventilated.append(file)
        tmp = [x for x in self.install_files_list if x not in ventilated]
        self.install_files_list = tmp

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
        pushdir(self._local_install_path(True))
        ventilated = []
        for file in self.install_files_list:
            libtype = _is_dynamic_library(file)
            if libtype == 'elf':
                soname = elf_soname(file)
                tmp = soname.split('.')
                tmp.remove('so')
                binpkgname = ''.join(tmp)  # libxxx.0.1.2 -> libxxx<ABI>
                pkg = self._binpkg_get_create(binpkgname, 'library')
                pkg.install_files.append(file)
                ventilated.append(file)

                # add the soname file to the same package
                so_filename = os.path.dirname(file) + '/' + soname
                pkg.install_files.append(so_filename)
                ventilated.append(so_filename)
            elif libtype == 'pe':
                errmsg = '{}: pe format not supported'.format(file)
                raise NotImplementedError(errmsg)

        tmplist = [x for x in self.install_files_list if x not in ventilated]
        self.install_files_list = tmplist
        popdir()

    def _get_fallback_package(self, bin_pkg_name: str) -> BinaryPackage:
        '''
        if a binary package is already created, use it
        if there is no binary package yet, try to fallback with a library pkg
        finally, create and fallback a binary package
        '''

        if bin_pkg_name in self._packages:
            return self._packages[bin_pkg_name]
        else:
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
        bin_pkg_name = self.name
        doc_pkg_name = self.name + '-doc'
        devel_pkg_name = self.name + '-devel'
        debug_pkg_name = self.name + '-debug'

        self._remove_ignored_files()
        self._ventilate_custom_packages()
        self._ventilate_pkg_create()

        tmplist = []
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
                # skip this. It will be put in a default fallback
                # package at the end of the ventilation process
                continue

            pkg.install_files.append(file)
            tmplist.append(file)

        tmplist = [x for x in self.install_files_list if x not in tmplist]
        self.install_files_list = tmplist

        # deal with the remaining files:
        if self.install_files_list:
            pkg = self._get_fallback_package(bin_pkg_name)
            pkg.install_files += self.install_files_list

    def generate_binary_packages(self):
        'create all the binary packages'
        instdir = self._local_install_path(True)
        pushdir(instdir)

        for pkgname, binpkg in self._packages.items():
            # TODO: generate dependencies
            binpkg.gen_sysdeps()
            binpkg.gen_provides()
            binpkg.create(instdir)
            iprint('generated package: {}'.format(pkgname))
        popdir()  # local install path

    def __repr__(self):
        return u'{}'.format(self.__dict__)
