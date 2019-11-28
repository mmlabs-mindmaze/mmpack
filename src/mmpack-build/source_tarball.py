# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import shutil
from tempfile import mkdtemp
from typing import Dict

import urllib3

from . common import *
from . workspace import Workspace


def _git_subcmd(subcmd: str, gitdir: str = '.git') -> str:
    cmd = 'git --git-dir={} {}'.format(gitdir, subcmd)
    return shell(cmd).strip()


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

    iprint('cloning {} into tmp dir {}'.format(url, clonedir))
    git_clone_sh_cmd = '{0} git clone {1} {2} {3}'\
                       .format(cmd_env, git_opts, url, clonedir)
    shell(git_clone_sh_cmd)


###########################################################################
#
#             Create mmpack source dir package
#
###########################################################################
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
        tag = _git_subcmd('rev-parse --abbrev-ref HEAD', git_dir)

    shutil.rmtree(git_dir, ignore_errors=True)

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


###########################################################################
#
#              Fetch upstream sources
#
###########################################################################
def _fetch_upstream_from_git(srcdir: str, specs: Dict[str, str]) -> str:
    """
    Fetch upstream sources from git clone

    Args:
        srcdir: folder where sources must be cloned
        specs: dict of settings put in source-strap file
    """
    _git_clone(specs['url'], srcdir, specs.get('branch'))
    shutil.rmtree(srcdir + '/.git')


def _fetch_upstream_from_tar(srcdir: str, specs: Dict[str, str]) -> str:
    """
    Fetch upstream sources from remote tar

    Args:
        srcdir: folder where sources must be extracted
        specs: dict of settings put in source-strap file
    """
    url = specs['url']
    filename = os.path.basename(url)
    downloaded_file = os.path.normpath(os.path.join(srcdir, '..', filename))

    # Download remote tar
    iprint('Downloading {}...'.format(url))
    request = urllib3.PoolManager().request('GET', url)
    with open(downloaded_file, 'wb') as outfile:
        outfile.write(request.data)
    iprint('Done')

    # Verify sha256 is correct if supplied in specs
    if 'sha256' in specs and specs['sha256'] != sha256sum(downloaded_file):
        raise Assert("Downloaded file does not match expected sha256")

    # Extract downloaded_file in srcdir
    with tarfile.open(downloaded_file, 'r:*') as tar:
        tar.extractall(path=srcdir)

    os.remove(downloaded_file)


def _fetch_upstream(srcdir: str, specs: Dict[str, str]):
    """
    Fetch upstream sources using specified method and extract it to src dir

    Args:
        srcdir: folder where sources must be extracted
        specs: dict of settings put in source-strap file
    """
    method_mapping = {
        'git': _fetch_upstream_from_git,
        'tar': _fetch_upstream_from_tar,
    }

    # check that mandatory keys are present in sources-strap file
    missings_keys = {'method', 'url'}.difference(specs)
    if missings_keys:
        raise Assert('missing mandatory keys in source-strap: {}'
                     .format(', '.join(missings_keys)))

    # Select proper _fetch_upstream_* function according to method entry
    method = specs['method']
    fetch_upstream_fn = method_mapping.get(method)
    if not fetch_upstream_fn:
        raise Assert("Invalid method " + method)

    # execute selected method
    fetch_upstream_fn(srcdir, specs)


###########################################################################
#
#              Source Tarball class
#
###########################################################################
class SourceTarball:
    """
    Class managing source tarball creation
    """

    def __init__(self, method: str, path_url: str, tag: str = None,
                 outdir: str = None, **kwargs):
        """
        Create a source package from various methods

        Args:
            method: 'tar', 'srcpkg' or 'git'
            path_url: path or url to the mmpack sources
            tag: the name of the commit/tag/branch to checkout (may be None)
            outdir: folder where to put the generated source package. If None,
                it will be located in Workspace().packages
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
        if not outdir:
            outdir = wrk.packages

        # Fetch sources following the specified method and move them to the
        # temporary source build folder
        dprint('extracting sources in the temporary directory: {}'
               .format(self._srcdir))
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

        self._process_source_strap()

        # Create source package tarball
        self.srctar = '{0}/{1}_{2}_src.tar.xz'.format(outdir, name, version)
        dprint('Building source tarball ' + self.srctar)
        create_tarball(self._srcdir, self.srctar, 'xz')

    def __del__(self):
        # If source build dir has been created and not detach, remove it at
        # instance destruction
        if self._srcdir:
            dprint('Destroying temporary source build dir ' + self._srcdir)
            shutil.rmtree(self._srcdir)

    def _process_source_strap(self):
        source_strap = os.path.join(self._srcdir, 'mmpack/sources-strap')
        try:
            specs = yaml_load(source_strap)
        except FileNotFoundError:
            return  # There is no source strap, nothing to be done

        upstream_srcdir = os.path.join(self._srcdir, 'mmpack/upstream')
        _fetch_upstream(upstream_srcdir, specs)

        # Move extracted upstream sources except mmpack packaging
        for elt in os.listdir(upstream_srcdir):
            if elt != 'mmpack':
                shutil.move(os.path.join(upstream_srcdir, elt), self._srcdir)

        # Clean leftover of temporary extracted
        shutil.rmtree(upstream_srcdir)

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
