# @mindmaze_header@
"""
Base class abstracting system package manager
"""

import os
from typing import List, Dict

from . workspace import cached_download
from . mm_version import Version


class SysPkg:
    """
    representation of a generic system package to download from repo
    """
    def __init__(self):
        self.name = None
        self.version = None
        self.source = None
        self.filename = None
        self.url = None
        self.sha256 = None

    def get_sysdep(self) -> str:
        """
        get system dependency string corresponding to depends on the package
        """
        return self.version

    def download(self, builddir) -> str:
        """
        get system dependency string corresponding to depends on the package
        """
        pkg_file = os.path.join(builddir, self.filename)
        cached_download(self.url, pkg_file, self.sha256)
        return pkg_file


class SysPkgManager:
    """
    Base class of interaction with system package manager
    """

    def find_sharedlib_sysdep(self, soname: str, symbols: List[str]) -> str:
        """
        Parses the installed system packages and find a library installed by
        the system package manager that provides the specified soname. All
        the symbols provided by the library found soname will be discarded
        from the provided list of symbols.

        Args:
            soname: SONAME of the shared library to find
            symbols: list of symbols used that have not be attributed to any
                library.

        Return: the system dependency found
        """
        raise NotImplementedError

    def find_pypkg_sysdep(self, pypkg: str) -> str:
        """
        Get sysdep providing the specified python package
        """
        raise NotImplementedError

    def _parse_pkgindex(self, builddir: str, srcname: str) -> List[SysPkg]:
        """
        fetch and parse system repository index and generate the list of system
        package that are created from the same source.

        Args:
            builddir: path where to download temporary files
            srcname: name of source project

        Return: List of system package description
        """
        raise NotImplementedError

    def _get_mmpack_version(self, sys_version: str) -> Version:
        """
        Get a version usable for a mmpack package from system package version
        """
        # pylint: disable=no-self-use
        return Version(sys_version)

    def _extract_syspkg(self, pkgfile: str, unpackdir: str) -> List[str]:
        """
        Extract downloaded system package file

        Args:
            pkgfile: path to downloaded system package file
            unpackdir: location where to unpack the downloaded system package

        Return: list of files unpacked from the system package
        """
        raise NotImplementedError

    def fetch_unpack(self, srcname: str, builddir: str,
                     unpackdir: str) -> (str, Dict[str, str]):
        """
        Download and unpack system package matching the source name

        Args:
            srcname: name of the source used to list the binary packages
            builddir: path where to download the temporary files (repo, pkgs)
            unpackdir: path where to unpack the matching system packages

        Return:
            Couple of system package version as downloaded and the mapping of
            file to package name that provides them
        """
        pkg_list = self._parse_pkgindex(builddir, srcname)
        if not pkg_list:
            raise FileNotFoundError('No system package matching {} found'
                                    .format(srcname))

        files_sysdep = dict()
        for pkg in pkg_list:
            # Download and extract the download system package file and update
            # the mapping files -> syspkg
            pkg_file = pkg.download(builddir)
            files = self._extract_syspkg(pkg_file, unpackdir)
            files_sysdep.update(dict.fromkeys(files, pkg.get_sysdep()))

        version = self._get_mmpack_version(pkg_list[0].version)
        return (version, files_sysdep)
