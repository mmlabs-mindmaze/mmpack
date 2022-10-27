# @mindmaze_header@
"""
template to create mmpack-build hook
"""

from typing import Dict, List

from .package_info import PackageInfo, DispatchData


class BaseHook:
    """
    Base class of mmpack-build hook
    """

    def __init__(self, srcname: str, host_archdist: str,
                 description: str, builddir: str):
        """
        Initialize the hook class

        Args:
            srcname: name of source package being built
            host_archdist: architecture/distribution of the host, ie which
                arch/dist the package is being built for
            description: description of the source package
            builddir: location where to put files during package build
        """
        self._srcname = srcname
        self._arch = host_archdist
        self._src_description = description
        self._builddir = builddir

    def post_local_install(self):
        """
        Immediately called after a project has been compiled and installed
        locally and before installed files are listed and dispatched in any
        binary package.  It can be used by hook implementation for example
        to rename/move installed files to ensure certain standard behavior
        are applied.

        When the hook method is called the current directory is the one
        where the files have been locally installed (install prefix)
        """
        # pylint: disable=no-self-use
        return

    def dispatch(self, data: DispatchData):
        """
        Called after a project has been compiled and installed locally.
        Look up in installed files and determine for each file, if handled
        by plugin, which package it must be assigned to.
        """
        # pylint: disable=unused-argument, no-self-use
        return

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

    def update_depends(self, pkg: PackageInfo, other_pkgs: List[PackageInfo]):
        """
        Look in files assigned to a binary package update the list of
        mmpack and system dependencies of the package in the fields of pkg.
        This hook method is called after the method update_provides() is
        called with all binary package cobuilded by the same projects.  In
        other words, the "provides" field in other_pkgs is complete when
        this method is called.
        """
        # pylint: disable=unused-argument, no-self-use
        return None
