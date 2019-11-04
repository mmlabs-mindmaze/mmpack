# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import shutil
from tempfile import mkdtemp

from . common import *
from . workspace import Workspace


def _git_clone(url: str, clonedir: str, tag: str = None,
               git_ssh_cmd: str = None):
    """
    create a shallow clone of a git repo

    Args:
        url: url of git repository
        clonedir: folder where the repo must be cloned into
        tag: option tag, branch, commit hash to check out
        git_ssh_cmd: optional, ssh cmd to use when cloning git repo through ssh
    """
    git_opts = '--quiet --depth=1'
    if tag:
        git_opts += ' --branch ' + tag

    if os.path.isdir(url):
        url = 'file://' + os.path.abspath(url)

    cmd_env = ''
    if git_ssh_cmd:
        cmd_env += 'GIT_SSH_COMMAND="{}"'.format(git_ssh_cmd)

    iprint('cloning ' + url)
    git_clone_sh_cmd = '{0} git clone {1} {2} {3}'\
                       .format(cmd_env, git_opts, url, clonedir)
    shell(git_clone_sh_cmd)


def _create_srcdir_from_git(builddir: str, url: str,
                            tag: str, **kwargs) -> str:
    """
    Create a source package folder from git clone

    Args:
        builddir: folder where sources must be cloned
        url: url of the git repository
        tag: the name of the commit/tag/branch to checkout (may be None)
        **kwargs: supported keyword arguments are following
            git_ssh_cmd: ssh cmd to use when cloning git repo through ssh

    Returns:
        tag that have been checked out.
    """
    _git_clone(url, builddir, tag, kwargs.get('git_ssh_cmd'))
    git_dir = builddir + '/.git'

    # Get tag name if not set yet (use current branch)
    if not tag:
        cmd = 'git --git-dir={} rev-parse --abbrev-ref HEAD'.format(git_dir)
        tag = shell(cmd).strip()

    shutil.rmtree(git_dir)

    return tag


def _create_srcdir_from_tar(builddir: str, filename: str,
                            tag: str, **kwargs) -> str:
    """
    Create a source package folder from tarball, not necessarily named
    following mmpack src tarball convention.

    Args:
        builddir: folder where sources must be extracted
        filename: filename of the tarball
        tag: the name of the tag (only transmitted in later stage), can be None
        **kwargs: keyword arguments (ignored, used on other method)

    Returns:
        `tag` if not None, 'from_par' otherwise
    """
    tar = tarfile.open(filename, 'r:*')
    tar.extractall(path=builddir)

    return tag if tag else 'from_tar'


def _create_srcdir(method: str, builddir: str,
                   path_url: str, tag: str, **kwargs) -> str:
    """
    Fetch source using specified method and extract it to src dir

    Args:
        method: 'tar', 'git' or srcpkg
        builddir: folder where sources must be extracted
        path_url: path or url to the mmpack sources
        tag: the name of the commit/tag/branch to checkout (may be None)
        **kwargs: supported keyword arguments passed to specific method
    """
    method_mapping = {
        'git': _create_srcdir_from_git,
        'tar': _create_srcdir_from_tar,
        'srcpkg': _create_srcdir_from_tar,
    }

    create_srcdir_fn = method_mapping.get(method)
    if not create_srcdir_fn:
        raise ValueError("Invalid method " + method)

    return create_srcdir_fn(builddir, path_url, tag, **kwargs)


class SourceTarball:
    """
    Class managing source tarball creation
    """

    def __init__(self, method: str, path_url: str, tag: str = None, **kwargs):
        """
        Create a source package from various methods

        Args:
            method: 'tar', 'srcpkg' or 'git'
            path_url: path or url to the mmpack sources
            tag: the name of the commit/tag/branch to checkout (may be None)
            **kwargs: supported keyword arguments are following
                git_ssh_cmd: ssh cmd to use when cloning git repo through ssh
        """
        # declare class instance attributes
        self._srcdir = None
        self.srctar = None
        self.tag = None
        self.name = None

        wrk = Workspace()
        self._srcdir = mkdtemp(dir=wrk.sources)
        outdir = wrk.packages

        # Fetch sources following the specified method and move them to the
        # temporary source build folder
        iprint('extracting temporarily to sources ' + self._srcdir)
        self.tag = _create_srcdir(method, self._srcdir,
                                  path_url, tag, **kwargs)

        # extract minimal metadata from package
        try:
            name, version = get_name_version_from_srcdir(self._srcdir)
            self.name = name
        except FileNotFoundError:  # raised if srcdir lack mmpack specs
            return

        # If the input was a mmpack source package, there is nothing else to do
        if method == 'srcpkg':
            self.srctar = path_url
            return

        # Create source package tarball
        self.srctar = '{0}/{1}_{2}_src.tar.gz'.format(outdir, name, version)
        dprint('Building source tarball ' + self.srctar)
        create_tarball(self._srcdir, self.srctar, 'gz')

    def __del__(self):
        # If source build dir has been created and not detach, remove it at
        # instance destruction
        if self._srcdir:
            dprint('Destroying temporary source build dir ' + self._srcdir)
            shutil.rmtree(self._srcdir)

    def detach_srcdir(self) -> str:
        """
        Remove ownership of extracted srcdir from SourceTarball and return it
        """
        srcdir = self._srcdir
        self._srcdir = None
        return srcdir

    def prepare_binpkg_build(self):
        """
        prepare folder for building the binary packages
        """
        wrk = Workspace()

        # Init workspace folders
        wrk.clean(self.name, self.tag)
        builddir = wrk.builddir(self.name, self.tag)
        unpackdir = os.path.join(builddir, self.name)

        iprint('moving unpacked sources from {0} to {1}'
               .format(self._srcdir, unpackdir))
        shutil.move(self._srcdir, unpackdir)

        # Copy package tarball in package builddir
        new_srctar = os.path.join(builddir, os.path.basename(self.srctar))
        shutil.copyfile(self.srctar, new_srctar)

        self._srcdir = unpackdir
        self.srctar = new_srctar
