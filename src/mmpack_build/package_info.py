# @mindmaze_header@
"""
interexchange data for package definition
"""

import re
from typing import Set, Tuple

from .common import extract_matching_set
from .mm_version import Version


_DEP_RE = re.compile(r'([a-zA-Z0-9-]+)\s*(?:\(\s*([<>=]+)\s*([^) ]+)\s*\))?')


def _unpack_dep_string(dep: str) -> Tuple[str, Version, Version]:
    """
    expected ependency syntax is:
        <name>
    or
        <name> (operator version)
    """
    match = _DEP_RE.fullmatch(dep.strip())
    if match:
        name, operator, version = match.groups()

        if not operator:
            return (name, Version('any'), Version('any'))
        elif operator == '=':
            return (name, Version(version), Version(version))
        elif operator == '>=':
            return (name, Version(version), Version('any'))
        elif operator == '<':
            return (name, Version('any'), Version(version))

    raise ValueError(f'Invalid dependency: "{dep}". It must respect the '
                     'format "depname [(>=|<|= version)]"')


def _unpack_dep_dict(dep: dict) -> Tuple[str, Version, Version]:
    """
    expected full mmpack dependency syntax is:
        <name>: [min_version, max_version]
        <name>: min_version
        <name>: any
    """
    if len(dep) > 1:
        raise ValueError(f'invalid dependency "{dep}"')

    name, val = dep.popitem()
    if isinstance(val, str):
        minv = val
        maxv = 'any'
    elif isinstance(val, (list, tuple)):
        minv = val[0]
        maxv = val[1]
    else:
        raise ValueError(f'invalid dependency "{dep}"')

    return (name, Version(minv), Version(maxv))


def _unpack_dep(dep) -> Tuple[str, Version, Version]:
    """
    helper to allow simpler mmpack dependency syntax

    expected full mmpack dependency syntax is:
        <name>
    or
        <name> (operator version)
    or
        <name>: [min_version, max_version]
        <name>: min_version
        <name>: any
    """
    if isinstance(dep, str):
        return _unpack_dep_string(dep)
    elif isinstance(dep, dict):
        return _unpack_dep_dict(dep)

    raise ValueError(f'Invalid dependency: "{dep}". It must respect '
                     '''the one of the following format:
                            <name>
                        or
                            <name> (operator version)
                        or
                            <name>: [min_version, max_version]
                            <name>: min_version
                            <name>: any
                     ''')


class PackageInfo:
    # pylint: disable=too-many-instance-attributes
    """
    container to share package data with hooks
    """

    def __init__(self, name: str):
        self.name = name
        self.files = set()
        self.provides = {}
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
        self.description = specs.get('description', '').strip()

        # Load depends
        for dep in specs.get('depends', []):
            self.deplist.append(_unpack_dep(dep))

        # Load host distribution specific system depends
        sysdeps_key = 'sysdepends-' + host_dist
        for dep in specs.get(sysdeps_key, []):
            self.add_sysdep(dep)

        # Populate files
        for regex in specs.get('files', []):
            self.files.update(extract_matching_set(regex, install_files))


class DispatchData:
    # pylint: disable=too-few-public-methods
    """
    Class holding information of files that need to be assigned and the package
    that are meant to be created
    """
    def __init__(self, files: Set[str]):
        self.unassigned_files = files
        self.pkgs = {}

    def assign_to_pkg(self, name: str, files: Set[str] = None) -> PackageInfo:
        """
        Returns the matching package info if one exist with the supplied name,
        create it otherwise.

        Args:
            name: name of package to create or update
            files: files to assign to package

        Return:
            the package info found or created
        """
        pkg = self.pkgs.get(name)
        if not pkg:
            # The package cannot be found in existing ones, let's add new one
            pkg = PackageInfo(name)
            self.pkgs[name] = pkg

        # Add files to package and remove them from install
        if files:
            pkg.files.update(files)
            self.unassigned_files.difference_update(files)

        return pkg
