# @mindmaze_header@
"""
Utils to use mmpack prefix in mmpack-build
"""

import os
from argparse import Namespace
from contextlib import contextmanager
from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, Iterable, List, Optional

from .common import popdir, pushdir, run_cmd
from .workspace import Workspace


class BuildPrefix(Enum):
    """Enumeration indicating how to use mmpack prefix during build"""
    NONE = 'none'
    CREATE = 'create'
    SUPPLIED = 'supplied'


@dataclass
class PrefixHandlingOptions:
    """Settings of how to use mmpack prefix"""
    repo_urls: List[str] = field(default_factory=list)
    use_build_prefix = BuildPrefix.NONE
    install_deps: bool = False

    def update_from_opts(self, opts: Namespace):
        """Update settings from cmgline options"""
        self.repo_urls = opts.repo_url

        # setup default value of use_build_prefix depending on other options
        if self.repo_urls:
            self.use_build_prefix = BuildPrefix.CREATE
        elif opts.prefix is not None:
            self.use_build_prefix = BuildPrefix.SUPPLIED
        else:
            self.use_build_prefix = BuildPrefix.NONE

        if opts.build_prefix is not None:
            self.use_build_prefix = BuildPrefix(opts.build_prefix)

        # setup default value of install deps
        self.install_deps = False
        if self.use_build_prefix is BuildPrefix.CREATE:
            self.install_deps = True

        # handle deprecated --build-deps option of pkg-create
        if 'build_deps' in opts and opts.build_deps is not None:
            self.install_deps = True

        if opts.install_deps is not None:
            self.install_deps = opts.install_deps


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


def configure_prefix_handling(opts: Namespace):
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


def run_build_script(name: str, execdir: str, specdir: str,
                     args: List[str] = None,
                     env: Optional[Dict[str, str]] = None):
    """
    Execute build script from spec dir

    Args:
        name: name of file script to execute
        execdir: path where the hook must be executed
        specdir: path where spec files should be found
        args: list of argument passed to script if not None
        env: environ variables to add in addition to the inherited env
    """
    # Run hook only if existing (try _hook suffix before giving up)
    script = os.path.join(specdir, name + '_script')
    if not os.path.exists(script):
        script = os.path.join(specdir, name + '_hook')
        if not os.path.exists(script):
            return

    hook_env = os.environ.copy()
    hook_env.update(env if env else {})

    cmd = ['sh', os.path.abspath(script)]
    cmd += args if args else []

    pushdir(os.path.abspath(execdir))
    run_cmd(cmd, env=hook_env)
    popdir()
