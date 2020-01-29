# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import os
import shutil
from tempfile import mkdtemp
from typing import Dict, Optional

from . common import *
from . workspace import Workspace, cached_download


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


class SourceTarball:
    """
    Class managing source tarball creation
    """
    def __init__(self, method: str, path_url: str, tag: str = None,
                 outdir: str = Workspace().packages, **kwargs):
        """
        Create a source package from various methods

        Args:
            method: 'tar', 'srcpkg', 'path' or 'git'
            path_url: path or url to the mmpack sources
            tag: the name of the commit/tag/branch to checkout (may be None)
            outdir: folder where to put the generated source package. If None,
                it will be located in Workspace().packages
            **kwargs: supported optional keyword arguments are following
                git_ssh_cmd: ssh cmd to use when cloning git repo through ssh
        """
        # declare class instance attributes
        self.srctar = None
        self.tag = tag
        self.name = None
        self._path_url = path_url
        self._kwargs = kwargs
        self._srcdir: Optional[str] = mkdtemp(dir=Workspace().sources)
        self.trace = dict()

        # Fetch sources following the specified method and move them to the
        # temporary source build folder
        dprint('extracting sources in the temporary directory: {}'
               .format(self._srcdir))
        self._create_srcdir(method)

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
        self._store_src_orig_tracing()

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

    def _get_unpacked_upstream_dir(self):
        """
        Get location when upstream source code is expected to be extracted if
        fetched. This does not created the folder.
        """
        return os.path.join(self._srcdir, 'mmpack/upstream')

    def _process_source_strap(self):
        source_strap = os.path.join(self._srcdir, 'mmpack/sources-strap')
        try:
            specs = yaml_load(source_strap)
        except FileNotFoundError:
            return  # There is no source strap, nothing to be done

        self._fetch_upstream(specs)

    def detach_srcdir(self) -> str:
        """
        Remove ownership of extracted srcdir from SourceTarball and return it
        """
        if not self._srcdir:
            raise Assert("Internal Error: source directory cannot be None")
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

    def _store_src_orig_tracing(self):
        """
        Write src_orig_tracing file from trace information
        """
        upstream_info = self.trace.get('upstream', {'method': 'in-src-pkg'})
        data = {'packaging': self.trace['pkg'], 'upstream': upstream_info}

        file_path = os.path.join(self._srcdir, 'mmpack/src_orig_tracing')
        yaml_serialize(data, file_path, use_block_style=True)

    def _create_srcdir_from_git(self):
        """
        Create a source package folder from git clone
        """
        git_ssh_cmd = self._kwargs.get('git_ssh_cmd')
        _git_clone(self._path_url, self._srcdir, self.tag, git_ssh_cmd)
        git_dir = self._srcdir + '/.git'

        # Get tag name if not set yet (use current branch)
        if not self.tag:
            self.tag = _git_subcmd('rev-parse --abbrev-ref HEAD', git_dir)

        commit_ref = _git_subcmd('rev-parse HEAD', git_dir)
        self.trace['pkg'].update({'url': self._path_url, 'ref': commit_ref})

        shutil.rmtree(git_dir, ignore_errors=True)

    def _create_srcdir_from_tar(self):
        """
        Create a source package folder from tarball, not necessarily named
        following mmpack src tarball convention.
        """
        tar = tarfile.open(self._path_url, 'r:*')
        tar.extractall(path=self._srcdir)

        # Get tag name if not set yet
        if not self.tag:
            self.tag = 'from_tar'

        self.trace['pkg'].update({'filename': self._path_url,
                                  'sha256': sha256sum(self._path_url)})

    def _create_srcdir_from_path(self):
        """
        Create a source package folder from folder.
        """
        # Copy each element of the source path folder. os.copytree cannot be
        # used for the whole dir because dirs_exist_ok option has been
        # introduced only in 3.8
        for dentry in os.listdir(self._path_url):
            elt_path = os.path.join(self._path_url, dentry)
            if os.path.isdir(elt_path):
                dstdir = os.path.join(self._srcdir, dentry)
                shutil.copytree(elt_path, dstdir, symlinks=True)
            else:
                shutil.copy(elt_path, self._srcdir, follow_symlinks=False)

        # Get tag name if not set yet
        if not self.tag:
            self.tag = 'from_path'

        self.trace['pkg'].update({'path': self._path_url})

    def _create_srcdir(self, method: str):
        """
        Fetch source using specified method and extract it to src dir

        Args:
            method: 'tar', 'git', 'path' or 'srcpkg'
        """
        method_mapping = {
            'git': self._create_srcdir_from_git,
            'path': self._create_srcdir_from_path,
            'tar': self._create_srcdir_from_tar,
            'srcpkg': self._create_srcdir_from_tar,
        }

        create_srcdir_callable = method_mapping.get(method)
        if not create_srcdir_callable:
            raise ValueError("Invalid method " + method)

        self.trace['pkg'] = {'method': method}
        create_srcdir_callable()

    def _fetch_upstream_from_git(self, specs: Dict[str, str]):
        """
        Fetch upstream sources from git clone

        Args:
            specs: dict of settings put in source-strap file
        """
        srcdir = self._get_unpacked_upstream_dir()
        gitdir = srcdir + '/.git'
        url = specs['url']
        _git_clone(url, srcdir, specs.get('branch'))

        gitref = _git_subcmd('rev-parse HEAD', gitdir)
        self.trace['upstream'].update({'url': url, 'ref': gitref})

        shutil.rmtree(gitdir)

    def _fetch_upstream_from_tar(self, specs: Dict[str, str]):
        """
        Fetch upstream sources from remote tar

        Args:
            specs: dict of settings put in source-strap file
        """
        url = specs['url']
        filename = os.path.basename(url)
        downloaded_file = os.path.join(self._srcdir, 'mmpack', filename)
        expected_sha256 = specs.get('sha256')

        cached_download(url, downloaded_file, expected_sha256)

        # Verify sha256 is correct if supplied in specs
        file_hash = sha256sum(downloaded_file)
        if expected_sha256 and expected_sha256 != file_hash:
            raise Assert("Downloaded file does not match expected sha256")

        self.trace['upstream'].update({'url': url, 'sha256': file_hash})

        # Extract downloaded_file in upstreamdir
        with tarfile.open(downloaded_file, 'r:*') as tar:
            tar.extractall(path=self._get_unpacked_upstream_dir())

        os.remove(downloaded_file)

    def _fetch_upstream(self, specs: Dict[str, str]):
        """
        Fetch upstream sources using specified method and extract it to src dir

        Args:
            specs: dict of settings put in source-strap file
        """
        method_mapping = {
            'git': self._fetch_upstream_from_git,
            'tar': self._fetch_upstream_from_tar,
        }

        # check that mandatory keys are present in sources-strap file
        missings_keys = {'method', 'url'}.difference(specs)
        if missings_keys:
            raise Assert('missing mandatory keys in source-strap: {}'
                         .format(', '.join(missings_keys)))

        # Select proper _fetch_upstream_* function according to method entry
        method = specs['method']
        fetch_upstream_callable = method_mapping.get(method)
        if not fetch_upstream_callable:
            raise Assert("Invalid method " + method)

        # execute selected method
        self.trace['upstream'] = {'method': method}
        fetch_upstream_callable(specs)

        # Determine in which subfolder of extracted upstream dir the source are
        # actually located
        upstream_srcdir = self._get_unpacked_upstream_dir()
        dir_content = list(os.listdir(upstream_srcdir))
        while len(dir_content) == 1:
            elt = os.path.join(upstream_srcdir, dir_content[0])
            if not os.path.isdir(elt):
                break
            upstream_srcdir = elt
            dir_content = os.listdir(upstream_srcdir)

        # Move extracted upstream sources except mmpack packaging
        for elt in dir_content:
            if elt != 'mmpack':
                shutil.move(os.path.join(upstream_srcdir, elt), self._srcdir)

        # Clean leftover of temporary extracted
        shutil.rmtree(self._get_unpacked_upstream_dir())
