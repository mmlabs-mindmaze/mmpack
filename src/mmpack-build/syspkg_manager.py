# @mindmaze_header@
"""
Helpers to interact with system package manager
"""

from .common import get_host_dist
from .syspkg_manager_base import SysPkg, SysPkgManager
from .syspkg_manager_dpkg import Dpkg
from .syspkg_manager_pacman_msys2 import PacmanMsys2


_SYSPKG_MGR = None


def _create_syspkg_manager() -> SysPkgManager:
    """
    Create the right system package manager class for the host
    """
    dist_manager_mapping = {
        'debian': Dpkg,
        'ubuntu': Dpkg,
        'windows': PacmanMsys2,
    }

    host_dist = get_host_dist()
    syspkg_manager = dist_manager_mapping.get(host_dist)
    if not syspkg_manager:
        raise ValueError('Unknown distribution: ' + host_dist)

    return syspkg_manager()


def get_syspkg_mgr() -> SysPkgManager:
    """
    get system package manager suitable for host target.
    """
    global _SYSPKG_MGR  # pylint: disable=global-statement

    if not _SYSPKG_MGR:
        _SYSPKG_MGR = _create_syspkg_manager()

    return _SYSPKG_MGR
