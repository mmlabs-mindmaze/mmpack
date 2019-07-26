# @mindmaze_header@
"""
template to create mmpack-build hook
"""

from typing import Set, Dict

from mm_version import Version


class PackageInfo:
    """
    container to share package data with hooks
    """

    def __init__(self, name: str):
        self.name = name
        self.files = set()
        self.provides = dict()
        self.deplist = []  # List of triplet of (dep_name, min_ver, max_ver)
        self.sysdeps = set()

    def add_to_deplist(self, pkgname: str,
                       minver: Version = Version('any'),
                       maxver: Version = Version('any')):
        """
        add a mmpack dependency optionally with a minimal and maximal
        version
        """
        self.deplist.append((pkgname, minver, maxver))

    def add_sysdep(self, sysdep: str):
        """
        add a system dependency
        """
        self.sysdeps.add(sysdep)


class BaseHook:
    """
    Base class of mmpack-build hook
    """

    def __init__(self, srcname: str, version: Version, host_archdist: str):
        """
        Initialize the hook class

        Args:
            srcname: name of source package being built
            version: version of source package being built
            host_archdist: architecture/distribution of the host, ie which
                arch/dist the package is being built for
        """
        self._srcname = srcname
        self._version = version
        self._arch = host_archdist

    def get_dispatch(self, install_files: Set[str]) -> Dict[str, Set[str]]:
        """
        Called after a project has been compiled and installed locally.
        Look up in installed files and determine for each file, if handled
        by plugin, which package it must be assigned to.
        """
        # pylint: disable=unused-argument, no-self-use
        return dict()

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        """
        Look in files assigned to a binary package and update the list of
        symbols/library exported (provided) by the package in pkg.
        """
        # pylint: disable=unused-argument, no-self-use
        return None

    def store_provides(self, pkg: PackageInfo, folder: str):
        """
        store provides data managed by hook
        """
        # pylint: disable=unused-argument, no-self-use
        return None
