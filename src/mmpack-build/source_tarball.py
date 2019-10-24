# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import shutil
from tempfile import mkdtemp

from . common import *
from . workspace import Workspace


def _git_clone(url: str, clonedir: str, tag: str = None):
    """
    create a shallow clone of a git repo

    Args:
        url: url of git repository
        clonedir: folder where the repo must be cloned into
        tag: option tag, branch, commit hash to check out
    """
    git_opts = '--quiet --depth=1'
    if tag:
        git_opts += ' --branch ' + tag

    if os.path.isdir(url):
        url = 'file://' + os.path.abspath(url)

    iprint('cloning ' + url)
    shell('git clone {0} {1} {2}'.format(git_opts, url, clonedir))


def _create_srcdir_from_git(builddir: str, url: str, tag: str) -> str:
    """
    Create a source package folder from git clone

    Args:
        builddir: folder where sources must be cloned
        url: url of the git repository
        tag: the name of the commit/tag/branch to checkout (may be None)

    Returns:
        tag that have been checked out.
    """
    _git_clone(url, builddir, tag)
    git_dir = builddir + '/.git'

    # Get tag name if not set yet (use current branch)
    if not tag:
        cmd = 'git --git-dir={} rev-parse --abbrev-ref HEAD'.format(git_dir)
        tag = shell(cmd).strip()

    shutil.rmtree(git_dir)

    return tag


def _create_srcdir_from_tar(builddir: str, filename: str, tag: str) -> str:
    """
    Create a source package folder from tarball, not necessarily named
    following mmpack src tarball convention.

    Args:
        builddir: folder where sources must be extracted
        filename: filename of the tarball
        tag: the name of the tag (only transmitted in later stage), can be None

    Returns:
        `tag` if not None, 'from_par' otherwise
    """
    tar = tarfile.open(filename, 'r:*')
    tar.extractall(path=builddir)

    return tag if tag else 'from_tar'


def _create_srcdir(method: str, builddir: str, path_url: str, tag: str) -> str:
    """
    Fetch source using specified method and extract it to src dir

    Args:
        method: 'tar' or 'git'
        builddir: folder where sources must be extracted
        path_url: path or url to the mmpack sources
        tag: the name of the commit/tag/branch to checkout (may be None)
    """
    method_mapping = {
        'git': _create_srcdir_from_git,
        'tar': _create_srcdir_from_tar,
    }

    create_srcdir_fn = method_mapping.get(method)
    if not create_srcdir_fn:
        raise ValueError("Invalid method " + method)

    return create_srcdir_fn(builddir, path_url, tag)


class SourceTarball:
    """
    Class managing source tarball creation
    """

    def __init__(self, method: str, path_url: str, tag: str = None):
        """
        Create a source package from various methods

        Args:
            method: 'tar' or 'git'
            path_url: path or url to the mmpack sources
            tag: the name of the commit/tag/branch to checkout (may be None)
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
        self.tag = _create_srcdir(method, self._srcdir, path_url, tag)

        # extract minimal metadata from package
        try:
            name, version = get_name_version_from_srcdir(self._srcdir)
            self.name = name
        except FileNotFoundError:  # raised if srcdir lack mmpack specs
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
