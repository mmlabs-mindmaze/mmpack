# @mindmaze_header@
"""
interexchange data for package definition
"""

from typing import Set, Dict

from . common import extract_matching_set
from . mm_version import Version


def _unpack_deps_version(item):
    """
    helper to allow simpler mmpack dependency syntax

    expected full mmpack dependency syntax is:
        <name>: [min_version, max_version]

    this allows the additional with implicit 'any' as maximum:
        <name>: min_version
        <name>: any
    """
    try:
        name, minv, maxv = item
        return (name, Version(minv), Version(maxv))
    except ValueError:
        name, minv = item  # if that fails, let the exception be raised
        minv = Version(minv)
        maxv = Version('any')
        return (name, minv, maxv)


class PackageInfo:
    # pylint: disable=too-many-instance-attributes
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
        self.description = ''

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

    def init_from_specs(self, specs, host_dist: str, install_files: Set[str]):
        """
        Init fields from custom package specification.
        """
        self.description = specs.get('description', '')

        # Load depends
        for dep in specs.get('depends', {}):
            item = list(dep.items())[0]
            name, minv, maxv = _unpack_deps_version(item)
            self.deplist.append((name, minv, maxv))

        # Load host distribution specific system depends
        sysdeps_key = 'sysdepends-' + host_dist
        for dep in specs.get(sysdeps_key, []):
            self.add_sysdep(dep)

        # Populate files
        for regex in specs.get('files', []):
            self.files.update(extract_matching_set(regex, install_files))


def pkginfo_get_create(name: str, pkgs: Dict[str, PackageInfo]) -> PackageInfo:
    """
    Returns the matching package info if one exist with the supplied name,
    create it otherwise.
    """
    pkg = pkgs.get(name)
    if not pkg:
        # The package cannot be found in existing ones, let's add new one
        pkg = PackageInfo(name)
        pkgs[name] = pkg

    return pkg
