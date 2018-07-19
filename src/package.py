# @mindmaze_header@
'''
TODOC
'''

import os
from glob import glob
import tarfile
import yaml
from workspace import Workspace
from common import shell, pushdir, popdir, yaml_serialize, ShellException, \
         iprint, eprint, remove_duplicates
from dependencies import dependencies
from elf_utils import elf_symbols_list
from version import Version


class Dependency(object):  # pylint: disable=too-few-public-methods
    '''
    TODOC
    '''
    def __init__(self, name: str, min_version: Version,
                 max_version: Version=None):
        '''
        only explicit deps on packages (script, python library ...)
        symbols deps will be automatically filled during package creation
        '''
        self.name = name
        self.min_version = min_version
        self.max_version = max_version


def guess_project_build_system() -> str:
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
    TODOC
    '''
    def __init__(self, name: str, tag: str, version: Version, source: str,
                 maintainer: str):
        # pylint: disable=too-many-arguments
        '''
        TODOC
        name, version, source (git link) and maintainer are mandatory
        others fields are optional

        tags is a list of strings
        dependencies is a list of Dependency objects
        '''
        self.name = name
        self.tag = tag
        self.version = version
        self.source = source
        self.maintainer = maintainer

        self.description = ''
        self.pkg_tags = ['MindMaze']
        self.dependencies = {'dpkg': [], 'mmpack': []}
        self.provides = {'elf': {}, 'pe': {}, 'python': {}}

        self._install_files_list = []
        self._metadata_files_list = []

    def _local_build_path(self) -> str:
        'internal helper: build and return the local build path'
        wrk = Workspace()
        return wrk.builddir(self.name) + '/{0}-{1}'.format(self.name, self.tag)

    def create_source_archive(self) -> str:
        ''' Create git archive (source package).

        Returns: the name of the archive file
        '''
        wrk = Workspace()

        pushdir(wrk.sources)
        sources_archive_name = '{0}-{1}-src.tar.gz'.format(self.name, self.tag)
        iprint('cloning ' + self.source)
        cmd = 'git clone --quiet ' + self.source
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
            build_system = guess_project_build_system()

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
        self._install_files_list = glob(os.getcwd() + '/**', recursive=True)
        popdir()

    def gen_provides(self) -> None:
        ''' Go through the install files, look for what this package provides

        eg. scan elf libraries for symbols
            python files for top-level classes and functions
            ...

        This fills the class provides field dictionary
        It does not return any value.

        Raises:
            ValueError: if a specified version is invalid or if a symbol in
                        the spec file is not found provided by the package
        '''
        provide_spec_name = '{0}/mmpack/{1}.provides.yaml' \
                            .format(self._local_build_path(), self.name)
        try:
            specs_provides = yaml.load(open(provide_spec_name, 'rb').read())
        except FileNotFoundError:
            specs_provides = {}

        for inst_file in self._install_files_list:
            self.provides['elf'].update(elf_symbols_list(inst_file,
                                                         self.version))

        for symbol, str_version in specs_provides['elf'].items():
            version = Version(str_version)  # will raise an error if malformed
            if symbol not in self.provides['elf']:
                raise ValueError('Specified elf symbol {0} not found '
                                 'in package files'.format(symbol))

            if version < self.version:
                self.provides['elf'][symbol] = version
            elif version > self.version:
                raise ValueError('Specified version of symbol {0} ({1})'
                                 'is greater than current version ({2})'
                                 .format(symbol, version, self.version))
            else:
                self.provides['elf'][symbol] = self.version

    def gen_dependencies(self) -> None:
        ''' Go through the install files and search for external dependencies

        FIXME: only handle elf deps ...
        '''
        for inst_file in self._install_files_list:
            self.dependencies['dpkg'].append(dependencies(inst_file))
        remove_duplicates(self.dependencies['dpkg'])

    def save_metadatas(self):
        ''' Saves package metadatas as yaml files

        generated files:
        - mmpack-provides-XXX.yaml
        - mmpack-dependencies-XXX.yaml
        - mmpack-XXX.yaml

        files are generated within the package install directory
        '''
        pushdir(self._local_build_path() + '/install')

        provides_filename = 'mmpack-provides-{0}.yaml'.format(self.name)
        yaml_serialize(self.provides, provides_filename)
        self._metadata_files_list.append(provides_filename)

        deps_filename = 'mmpack-dependencies-{0}.yaml'.format(self.name)
        yaml_serialize(self.dependencies, deps_filename)
        self._metadata_files_list.append(deps_filename)

        pkg_metadata_filename = 'mmpack-' + self.name + '.yaml'
        yaml_serialize(self.__dict__, pkg_metadata_filename)
        self._metadata_files_list.append(pkg_metadata_filename)
        popdir()

    def build_package(self) -> str:
        ''' create package archive

        the package (currently a tar archive) is an archive of both
         - the data to be installed
         - the metadata about the data to be installed

        put the generated package into the "package" directory

        Returns:
            a tuple of names of the generated packages files
            eg. (XXX, XXX-doc, XXX-dev, XXX-dbgsym)
        '''
        wrk = Workspace()
        prefix = self._local_build_path() + '/install'
        prefix_len = len(prefix)
        pushdir(prefix)

        pkg_data_name = '{0}-{1}.data.mmpack'.format(self.name, self.tag)
        pkg_name = '{0}-{1}.mmpack'.format(self.name, self.tag)

        # autodetect and generate those packages
        doc_pkg = None
        dev_pkg = None
        dbgsym_pkg = None

        pkg = tarfile.TarFile(name=pkg_data_name, mode='w')
        for inst_file in self._install_files_list:
            # convert to relative path
            inst_file = '.' + inst_file[prefix_len:]
            if inst_file:
                pkg.add(inst_file)
        pkg.close()

        # create and add <pkg_data_name> hash file
        # create and add signature file

        pkg = tarfile.TarFile(name=pkg_name, mode='w')
        pkg.add(pkg_data_name)
        for meta_file in self._metadata_files_list:
            pkg.add(meta_file)
        pkg.close()

        # copy package to
        cmd = 'cp -vf {0} {1}'.format(pkg_name, wrk.packages)
        shell(cmd)
        popdir()  # local install directory

        return (pkg_name, doc_pkg, dev_pkg, dbgsym_pkg)

    def __repr__(self):
        return u'{}'.format(self.__dict__)
