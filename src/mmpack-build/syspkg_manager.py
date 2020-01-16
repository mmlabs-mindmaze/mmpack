# @mindmaze_header@
"""
Helpers to interact with system package manager
"""

from . syspkg_manager_dpkg import Dpkg
from . syspkg_manager_pacman import Pacman


SYSPKG_MGR = None


def init_syspkg_manager(host_dist: str):
    """
    Setup the right system package manager class for the host
    """
    global SYSPKG_MGR  # pylint: disable=global-statement

    if host_dist in ('debian', 'ubuntu'):
        SYSPKG_MGR = Dpkg()
    elif host_dist == 'windows':
        SYSPKG_MGR = Pacman()
    else:
        raise ValueError('Unknown distribution: ' + host_dist)
