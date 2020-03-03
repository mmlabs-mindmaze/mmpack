# @mindmaze_header@
"""
interexchange data for package definition
"""

from . mm_version import Version


class PackageInfo:
    """
    container to share package data with hooks
    """

    def __init__(self, name: str):
        self.name = name
        self.files = set()
        self.provides = dict()
        self.deplist = []  # List of triplet of (dep_name, min_ver, max_ver)
        self.version = Version('any')
        self.sysdeps = set()
        self.ghost = False

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
