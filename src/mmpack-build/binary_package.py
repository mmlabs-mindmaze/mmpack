# @mindmaze_header@
"""
Class to handle binary packages, their dependencies, symbol interface, and
packaging as mmpack file.
"""

import os

from glob import glob
from os.path import isfile
from typing import List, Dict

from . base_hook import PackageInfo
from . common import *
from . hooks_loader import MMPACK_BUILD_HOOKS
from . mm_version import Version
from . workspace import get_staging_dir


class BinaryPackage:
    # pylint: disable=too-many-instance-attributes
    """
    Binary package class
    """

    def __init__(self, name: str, version: Version, source: str, arch: str,
                 tag: str, spec_dir: str, src_hash: str):
        # pylint: disable=too-many-arguments
        self.name = name
        self.version = version
        self.source = source
        self.arch = arch
        self.tag = tag
        self.spec_dir = spec_dir
        self.src_hash = src_hash
        self.pkg_path = None
        self.ghost = False

        self.description = ''
        # * System dependencies are stored as opaque strings.
        #   Those are supposed to be handles by system tools,
        #   => format is a set of strings.
        # * mmpack dependencies are expressed as the triplet
        #   dependency name, min and max version (inclusive)
        #   => format is a dict {depname: [min, max], ...}
        self._dependencies = {'sysdepends': set(), 'depends': {}}
        self.provides = {}
        self.install_files = set()

    def licenses_dir(self):
        """
        return the license directory of the package
        """
        licenses_dir = 'share/licenses/{}'.format(self.name)
        os.makedirs(licenses_dir, exist_ok=True)
        return licenses_dir

    def _get_specs_provides(self) -> Dict[str, Dict[str, Version]]:
        """
        return a dict containing the specified interface of given package

        Look for a <self.name>.provides file within the project's mmpack
        folder, load it and return its parsed values as a dictionary.

        return an empty interface dict if none was specified.
        """
        # TODO: also work with the last package published
        provide_spec_name = '{}/{}.provides'.format(self.spec_dir, self.name)
        dprint('reading symbols from ' + provide_spec_name)
        specs_provides = dict()
        try:
            specs_provides.update(yaml_load(provide_spec_name))
        except FileNotFoundError:
            # return an empty dict if nothing has been provided
            pass

        return specs_provides

    def _sha256sums_file(self) -> str:
        os.makedirs('var/lib/mmpack/metadata/', exist_ok=True)
        return 'var/lib/mmpack/metadata/{}.sha256sums'.format(self.name)

    def _gen_info(self, pkgdir: str):
        """
        This generate the info file and sha256sums. It must be the last step
        before calling _make_archive().
        """
        pushdir(pkgdir)

        # Compute hashes of all installed files
        cksums = {}
        for filename in glob('**', recursive=True):
            # skip folder and MMPACK/info
            if not isfile(filename) or filename == 'MMPACK/info':
                continue

            # Add file with checksum
            cksums[filename] = sha256sum(filename, follow_symlink=False)

        # Write the file sha256sums file
        with open(self._sha256sums_file(), 'wt', newline='\n') as sums_file:
            for filename in sorted(cksums):
                sums_file.write('{}: {}\n'.format(filename, cksums[filename]))

        # Create info file
        info = {'version': self.version,
                'source': self.source,
                'description': self.description,
                'srcsha256': self.src_hash,
                'sumsha256sums': sha256sum(self._sha256sums_file())}
        info.update(self._dependencies)
        yaml_serialize({self.name: info}, 'MMPACK/info')
        popdir()

    def _store_provides(self, pkgdir: str):
        pushdir(pkgdir)

        metadata_folder = 'var/lib/mmpack/metadata/'
        os.makedirs(metadata_folder, exist_ok=True)

        pkginfo = self.get_pkginfo()
        for hook in MMPACK_BUILD_HOOKS:
            hook.store_provides(pkginfo, metadata_folder)

        popdir()

    def _populate(self, instdir: str, pkgdir: str):
        for instfile in self.install_files:
            src = instdir + '/' + instfile
            dst = pkgdir + '/' + instfile
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            os.link(src, dst, follow_symlinks=False)

    def _make_archive(self, pkgdir: str, dstdir: str) -> str:
        mpkfile = "{0}/{1}_{2}_{3}.mpk".format(dstdir, self.name,
                                               self.version, self.arch)
        dprint('[tar] {0} -> {1}'.format(pkgdir, mpkfile))
        create_tarball(pkgdir, mpkfile, 'xz')

        return mpkfile

    def create(self, instdir: str, pkgbuilddir: str) -> str:
        """
        Gather all the package data, generates metadata files
        (including exposed symbols), and create the mmpack package
        file.

        Args:
            instdir: folder from which the package must be populated
            pkgbuilddir: build folder of source package. The staging file
                for the package as well as the created binary package file
                will be located in this folder.

        Returns:
            the path of the created binary package file (in pkgbuilddir)
        """
        stagedir = get_staging_dir(pkgbuilddir, self.name)
        os.makedirs(stagedir + '/MMPACK', exist_ok=True)

        dprint('link {0} in {1}'.format(self.name, stagedir))
        self._populate(instdir, stagedir)
        self._store_provides(stagedir)
        self._gen_info(stagedir)
        self.pkg_path = self._make_archive(stagedir, pkgbuilddir)
        return self.pkg_path

    def add_depend(self, name: str, minver: Version,
                   maxver: Version = Version('any')):
        """
        Add mmpack package as a dependency with a minimal version
        """
        # Drop dependencies to self
        if name == self.name:
            return

        dependencies = self._dependencies['depends']
        if name not in dependencies:
            dependencies[name] = [minver, maxver]
        else:
            curr_minver, curr_maxver = dependencies.get(name)
            minver = max(curr_minver, minver)
            maxver = min(curr_maxver, maxver)
            dependencies[name] = [minver, maxver]

    def add_sysdepend(self, opaque_dep: str):
        """
        Add a system dependencies to the binary package
        """
        self._dependencies['sysdepends'].add(opaque_dep)

    def get_pkginfo(self) -> PackageInfo:
        'Create PackageInfo instance out of binary package'
        pkginfo = PackageInfo(self.name)
        pkginfo.files = self.install_files
        pkginfo.provides = self.provides
        pkginfo.ghost = self.ghost
        return pkginfo

    def gen_provides(self):
        """
        Go through the install files, look for what this package provides

        eg. scan elf libraries for symbols
            python files for top-level classes and functions
            ...

        This fills the class provides field dictionary
        It does not return any value.

        Raises:
            ValueError: if a specified version is invalid or if a symbol in
                        the spec file is not found provided by the package
        """
        specs_provides = self._get_specs_provides()
        pkginfo = self.get_pkginfo()
        for hook in MMPACK_BUILD_HOOKS:
            hook.update_provides(pkginfo, specs_provides)

    def _find_link_deps(self, target: str, binpkgs: List['BinaryPackages']):
        for pkg in binpkgs:
            if target in pkg.install_files and pkg.name != self.name:
                self.add_depend(pkg.name, pkg.version, pkg.version)

    def gen_dependencies(self, binpkgs: List['BinaryPackages']):
        """
        Go through the install files and search for dependencies.
        """
        for inst_file in self.install_files:
            if os.path.islink(inst_file):
                target = os.path.join(os.path.dirname(inst_file),
                                      os.readlink(inst_file))
                self._find_link_deps(target, binpkgs)
                continue

        # Gather mmpack and system dependencies by executing each hook
        other_pkgs = [pkg.get_pkginfo() for pkg in binpkgs]
        currpkg = self.get_pkginfo()
        for hook in MMPACK_BUILD_HOOKS:
            hook.update_depends(currpkg, other_pkgs)

        for dep, minver, maxver in currpkg.deplist:
            self.add_depend(dep, minver, maxver)

        for sysdep in currpkg.sysdeps:
            self.add_sysdepend(sysdep)
