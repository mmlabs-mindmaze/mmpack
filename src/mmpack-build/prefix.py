# @mindmaze_header@
"""
Utils to use mmpack prefix in mmpack-build
"""

from contextlib import contextmanager
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


@contextmanager
def new_mmpack_prefix_context(path: str):
    """execute the block in a new mmpack prefix context.

    The meaning of the new context depends on the options passed to the
    mmpack-build command.
        - If the repository url list has been set, a new mmpack prefix is
          created in the directory `path`.
        - Otherwise, if Workspace().prefix has been previously set, it will be
          kept.

    Args:
        path: location of the mmpack prefix if it must be created
    """
    wrk = Workspace()
    previous_prefix = wrk.prefix

    if _REPO_URL_LIST:
        wrk.set_prefix(path)
        prefix_create(_REPO_URL_LIST)

    try:
        yield
    finally:
        wrk.prefix = previous_prefix
