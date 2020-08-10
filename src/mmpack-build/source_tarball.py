# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import os
import shutil
import tarfile
from collections import namedtuple
from subprocess import call, DEVNULL
from typing import Dict, Iterator

from . common import *
from . workspace import Workspace, cached_download, find_project_root_folder


ProjectSource = namedtuple('ProjectSource',
                           ['name', 'version', 'tarball', 'srcdir'])


def _is_git_dir(path: str) -> bool:
    if os.path.isdir(path + '/.git'):
        return True

    retcode = call(['git', '--git-dir='+path, 'rev-parse', '--git-dir'],
                   stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
    return retcode == 0


def _is_tarball_file(path: str) -> bool:
    try:
        with tarfile.open(path, 'r') as tar:
            return 'mmpack/specs' in tar.getnames()
    # pylint: disable=broad-except
    except Exception:
        return False


def _guess_method(path_or_url: str) -> str:
    if _is_git_dir(path_or_url):
        return 'git'
    elif os.path.isdir(path_or_url):
        return 'path'
    elif _is_tarball_file(path_or_url):
        if path_or_url.endswith('_src.tar.xz'):
            return 'srcpkg'
        else:
            return 'tar'

    # If nothing has worked before, lets assume this is a git remote url
    return 'git'


def _git_subcmd(subcmd: List[str], gitdir: str = None, worktree: str = None,
                git_ssh_cmd: str = None) -> str:
    env = None
    if gitdir or worktree or git_ssh_cmd:
        env = os.environ.copy()
        if gitdir:
            env['GIT_DIR'] = gitdir
        if worktree:
            env['GIT_WORK_TREE'] = worktree
        if git_ssh_cmd:
            env['GIT_SSH_COMMAND'] = git_ssh_cmd

    args = subcmd.copy()
    args.insert(0, 'git')
    return shell(args, env=env).strip()


def _git_clone(url: str, worktree: str, gitdir: str = None,
               refspec: str = None, git_ssh_cmd: str = None):
    """
    create a shallow clone of a git repo

    Args:
        url: url of git repository
        worktree: folder where the repo must be checked out into
        gitdir: optional folder that will hold the git repository.
        refspec: tag, branch, commit hash to check out
        git_ssh_cmd: optional, ssh cmd to use when cloning git repo through ssh
    """
    if os.path.isdir(url):
        url = 'file://' + os.path.abspath(url)

    if not refspec:
        refspec = 'HEAD'

    if not gitdir and worktree:
        gitdir = worktree + '/.git'

    iprint('cloning {} ({}) into tmp dir {}'.format(url, refspec, worktree))

    # Create and init git repository
    _git_subcmd(['init', '--quiet'], gitdir)

    # Fetch git refspec
    _git_subcmd(['fetch', '--quiet', '--depth=1', url, refspec],
                gitdir, worktree, git_ssh_cmd)

    # Checkout the specified refspec
    _git_subcmd(['checkout', '--quiet', '--detach', 'FETCH_HEAD'],
                gitdir, worktree)


# pylint: disable=too-many-instance-attributes
class SourceTarball:
    """
    Class managing source tarball creation
    """
    def __init__(self,
                 method: str = 'guess',
                 path_url: str = None,
                 tag: str = None,
                 outdir: str = None,
                 **kwargs):
        """
        Create a source package from various methods

        Args:
            method: 'tar', 'srcpkg', 'path' or 'git'. If 'guess' or None, the
                method will be guessed.
            path_url: path or url to the mmpack sources. If None, the path to
                mmpack project will be search in the current directory or one
                of its parent.
            tag: the name of the commit/tag/branch to checkout (may be None)
            outdir: folder where to put the generated source package. If None,
                it will be located in Workspace().outdir()
            **kwargs: supported optional keyword arguments are following
                git_ssh_cmd: ssh cmd to use when cloning git repo through ssh
        """
        if not path_url:
            path_url = find_project_root_folder(find_multiproj=True)
            if not path_url:
                raise ValueError('did not find project to package')

        if not method or method == 'guess':
            method = _guess_method(path_url)

        # declare class instance attributes
        self.srctar = None
        self.tag = tag
        self._method = method
        self._path_url = path_url
        self._kwargs = kwargs
        self._builddir = Workspace().tmpdir()
        self._srcdir = None
        self._vcsdir = None
        self._outdir = outdir if outdir else Workspace().outdir()
        self._prj_src = None
        self.trace = dict()

        # Fetch sources following the specified method and move them to the
        # temporary source build folder
        dprint('extracting sources in the temporary directory: {}'
               .format(self._builddir))
        self._create_srcdir(method)

        # Try generate the source from the root folder of project
        try:
            self._prj_src = self._gen_project_sources()
            self.srctar = self._prj_src.tarball
        except FileNotFoundError:  # raised if srcdir lack mmpack specs
            return

    def __del__(self):
        # If source build dir has been created and not detach, remove it at
        # instance destruction
        dprint('Destroying temporary source build dir ' + self._builddir)
        shutil.rmtree(self._builddir)

    def _gen_project_sources(self, subdir: str = '') -> ProjectSource:
        srcdir = self._srcdir
        if subdir:
            self.trace['pkg']['subdir'] = subdir
            srcdir += '/' + subdir

        # extract minimal metadata from package
        name, version = get_name_version_from_srcdir(srcdir)
        srctar = '{0}/{1}_{2}_src.tar.xz'.format(self._outdir, name, version)

        if self._method == 'srcpkg':
            # If the input was a mmpack source package, there is nothing else
            # to do besides copy the mmpack source tarball
            dprint('Copying mmpack source tarball ' + self._path_url)
            shutil.copy(self._path_url, srctar)
        else:
            self._process_source_strap(srcdir)
            self._store_src_orig_tracing(srcdir)

            # Create source package tarball
            dprint('Building source tarball ' + srctar)
            create_tarball(srcdir, srctar, 'xz')

        return ProjectSource(name=name,
                             version=version,
                             tarball=srctar,
                             srcdir=srcdir)

    def _get_unpacked_upstream_dir(self):
        """
        Get location when upstream source code is expected to be extracted if
        fetched. This does not created the folder.
        """
        return os.path.join(self._builddir, 'upstream')

    def _process_source_strap(self, srcdir):
        source_strap = os.path.join(srcdir, 'mmpack/sources-strap')
        try:
            specs = yaml_load(source_strap)
        except FileNotFoundError:
            return  # There is no source strap, nothing to be done

        self._fetch_upstream(specs, srcdir)

    def get_srcdir(self) -> str:
        """
        get directory of extracted source
        """
        return self._srcdir

    def iter_mmpack_srcs(self) -> Iterator[ProjectSource]:
        """
        Get an iterator of sources of all project referenced in the repository
        """
        # If the extracted/cloned data contains a mmpack packaging at the root
        # folder, this is the only project to return
        if self._prj_src:
            yield self._prj_src
            return

        try:
            prjlist_path = self._srcdir + '/projects.mmpack'
            subdirs = [l.strip() for l in open(prjlist_path, 'rt')]
        except FileNotFoundError:
            return

        for subdir in subdirs:
            yield self._gen_project_sources(subdir)

    def _store_src_orig_tracing(self, srcdir: str):
        """
        Write src_orig_tracing file from trace information
        """
        upstream_info = self.trace.get('upstream', {'method': 'in-src-pkg'})
        data = {'packaging': self.trace['pkg'], 'upstream': upstream_info}

        file_path = os.path.join(srcdir, 'mmpack/src_orig_tracing')
        yaml_serialize(data, file_path, use_block_style=True)

    def _create_srcdir_from_git(self):
        """
        Create a source package folder from git clone
        """
        self._vcsdir = self._builddir + '/vcsdir.git'
        git_ssh_cmd = self._kwargs.get('git_ssh_cmd')
        _git_clone(url=self._path_url,
                   worktree=self._srcdir,
                   gitdir=self._vcsdir,
                   refspec=self.tag,
                   git_ssh_cmd=git_ssh_cmd)

        # Get tag name if not set yet (use current branch)
        if not self.tag:
            self.tag = _git_subcmd(['rev-parse', '--abbrev-ref', 'HEAD'],
                                   self._vcsdir)

        commit_ref = _git_subcmd(['rev-parse', 'HEAD'], self._vcsdir)
        self.trace['pkg'].update({'url': self._path_url, 'ref': commit_ref})

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
        self._srcdir = self._builddir + '/src'
        os.mkdir(self._srcdir)

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
        gitdir = self._builddir + '/upstream.git'
        url = specs['url']

        os.mkdir(srcdir)

        _git_clone(url=url,
                   worktree=srcdir,
                   gitdir=gitdir,
                   refspec=specs.get('branch'))

        gitref = _git_subcmd(['rev-parse', 'HEAD'], gitdir=gitdir)
        self.trace['upstream'].update({'url': url, 'ref': gitref})

    def _fetch_upstream_from_tar(self, specs: Dict[str, str]):
        """
        Fetch upstream sources from remote tar

        Args:
            specs: dict of settings put in source-strap file
        """
        url = specs['url']
        filename = os.path.basename(url)
        downloaded_file = os.path.join(self._builddir, filename)
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

    def _fetch_upstream(self, specs: Dict[str, str], srcdir: str):
        """
        Fetch upstream sources using specified method and extract it to src dir

        Args:
            specs: dict of settings put in source-strap file
        """
        method_mapping = {
            'git': self._fetch_upstream_from_git,
            'tar': self._fetch_upstream_from_tar,
        }

        # Remove left over of upstream of other projects
        upstream_srcdir = self._get_unpacked_upstream_dir()
        shutil.rmtree(upstream_srcdir)

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
                shutil.move(os.path.join(upstream_srcdir, elt), srcdir)
