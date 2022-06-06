# @mindmaze_header@
"""
Utils to use mmpack prefix in mmpack-build
"""

from typing import Iterable, List

from .common import run_cmd
from .workspace import Workspace


_REPO_URL_LIST: List[str] = []


def _mmpack_cmd() -> List[str]:
    wrk = Workspace()
    return [wrk.mmpack_bin(), '--prefix=' + wrk.prefix]


def prefix_create(repo_url: Iterable[str]):
    """Create mmpack prefix with supplied repositories"""
    cmd = _mmpack_cmd()
    run_cmd(cmd + ['mkprefix', '--clean-repolist'])
    for idx, url in enumerate(repo_url):
        run_cmd(cmd + ['repo', 'add', f'repo{idx}', url])


def prefix_install(pkgs: Iterable[str]):
    """install packages in mmpack prefix"""
    cmd = _mmpack_cmd()
    run_cmd(cmd + ['update'])
    run_cmd(cmd + ['install', '-y'] + list(pkgs))


def cmd_in_optional_prefix(args: List[str]) -> List[str]:
    """
    Return list of args to run command in mmpack prefix if prefix set,
    otherwise directly.
    """
    if Workspace().prefix:
        return _mmpack_cmd() + ['run'] + args

    return args


def set_repo_url(repo_url: List[str]):
    """Store repo url list"""
    global _REPO_URL_LIST  # pylint: disable=global-statement
    _REPO_URL_LIST = repo_url
