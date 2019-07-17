# @mindmaze_header@
"""
template to create mmpack-build hook
"""

from typing import Set, Dict

from mm_version import Version


class BaseHook:
    # pylint: disable=too-few-public-methods
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
