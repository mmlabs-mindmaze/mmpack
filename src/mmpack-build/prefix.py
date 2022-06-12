# @mindmaze_header@
"""
Utils to use mmpack prefix in mmpack-build
"""

from argparse import Namespace
from contextlib import contextmanager
from dataclasses import dataclass, field
from enum import Enum
from typing import Iterable, List

from .common import run_cmd
from .workspace import Workspace


class BuildPrefix(Enum):
    NONE = 'none'
    CREATE = 'create'
    SUPPLIED = 'supplied'


@dataclass
class PrefixHandlingOptions:
    repo_urls: List[str] = field(default_factory=list)
    use_build_prefix = BuildPrefix.NONE
    install_deps: bool = False

    def update_from_opts(self, opts: Namespace):
        self.repo_urls = opts.repo_url
        self.install_deps = (opts.repo_url and opts.build_deps)

        # setup default value of use_build_prefix depending on other options
        if self.repo_urls:
            self.use_build_prefix = BuildPrefix.CREATE
        elif opts.prefix is not None:
            self.use_build_prefix = BuildPrefix.SUPPLIED
        else:
            self.use_build_prefix = BuildPrefix.NONE

        if opts.build_prefix is not None:
            self.use_build_prefix = BuildPrefix(opts.build_prefix)


_PREFIX_OPTIONS = PrefixHandlingOptions()


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
    install_list = list(pkgs)
    if not (install_list and _PREFIX_OPTIONS.install_deps):
        return

    cmd = _mmpack_cmd()
    run_cmd(cmd + ['update'])
    run_cmd(cmd + ['install', '-y'] + install_list)


def cmd_in_optional_prefix(args: List[str]) -> List[str]:
    """
    Return list of args to run command in mmpack prefix if prefix set,
    otherwise directly.
    """
    if _PREFIX_OPTIONS.use_build_prefix is not BuildPrefix.NONE:
        return _mmpack_cmd() + ['run'] + args

    return args


def update_prefix_handling_from_opts(opts: Namespace):
    """Update prefix options"""
    _PREFIX_OPTIONS.update_from_opts(opts)


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

    if _PREFIX_OPTIONS.use_build_prefix is BuildPrefix.CREATE:
        wrk.set_prefix(path)
        prefix_create(_PREFIX_OPTIONS.repo_urls)

    try:
        yield
    finally:
        wrk.prefix = previous_prefix
