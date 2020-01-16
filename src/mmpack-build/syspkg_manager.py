# @mindmaze_header@
"""
Helpers to interact with system package manager
"""

from . syspkg_manager_base import SysPkgManager
from . syspkg_manager_dpkg import Dpkg
from . syspkg_manager_msys2 import Msys2


_SYSPKG_MGR = None


def init_syspkg_manager(host_dist: str):
    """
    Setup the right system package manager class for the host

    Args:
        host_dist: target host distribution
    """
    global _SYSPKG_MGR  # pylint: disable=global-statement

    if host_dist in ('debian', 'ubuntu'):
        _SYSPKG_MGR = Dpkg()
    elif host_dist == 'windows':
        _SYSPKG_MGR = Msys2()
    else:
        raise ValueError('Unknown distribution: ' + host_dist)


def get_syspkg_mgr() -> SysPkgManager:
    """
    get system package manager suitable for host target.
    init_syspkg_manager() must have been called at least once beforehand.

    Return: the system package manager

    Raise:
        AssertionError: init_syspkg_manager() has been called before.
    """
    if not _SYSPKG_MGR:
        raise Assert('System package not initialized yet')

    return _SYSPKG_MGR
