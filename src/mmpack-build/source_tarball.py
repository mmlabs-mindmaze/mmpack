# @mindmaze_header@
"""
Fetch/gather sources of a mmpack package and create source tarball
"""

import os
import re
import shutil
from copy import copy
from os.path import abspath, basename, exists, join as join_path
from subprocess import call, DEVNULL
from tarfile import open as taropen, TarFile, TarInfo
from typing import Dict, Iterator, List, NamedTuple, Optional

from .common import *
from .errors import DownloadError, MMPackBuildError, ShellException
from .workspace import Workspace, cached_download, find_project_root_folder


class ProjectSource(NamedTuple):
    """
    Project source to be build
    """
    name: str
    version: str
    tarball: str
    srcdir: str


def _strip_leading_comp_tar_iter(tar: TarFile) -> Iterator[TarInfo]:
    for info in tar.getmembers():
        info = copy(info)

        # Strip leading directory component
        info.name = info.name.split('/', maxsplit=1)[-1]

        yield info


def _is_git_dir(path: str) -> bool:
    if not os.path.isdir(path):
        return False

    if os.path.isdir(path + '/.git'):
        return True

    retcode = call(['git', '--git-dir='+path, 'rev-parse', '--git-dir'],
                   stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
    return retcode == 0


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


# pylint: disable=too-many-arguments
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
    fetch_args = ['fetch', '--quiet', '--tags', url, refspec]
    _git_subcmd(fetch_args, gitdir, worktree, git_ssh_cmd)

    # Checkout the specified refspec
    _git_subcmd(['checkout', '--quiet', '--detach', 'FETCH_HEAD'],
                gitdir, worktree)


def _git_modified_files(gitdir: str, commit: str = 'HEAD') -> List[str]:
    """
    get list of file modified by check
    """
    args = ['diff-tree', '--no-commit-id', '--name-only', '-r', commit]
    return _git_subcmd(args, gitdir).splitlines()


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
                raise MMPackBuildError('did not find project to package')

        # declare class instance attributes
        self.tag = tag
        self._method = method if method else 'guess'
        self._path_url = path_url
        self._kwargs = kwargs
        self._builddir = Workspace().tmpdir()
        self._downloaded_file = None
        self._srcdir = None
        self._vcsdir = None
        self._outdir = outdir if outdir else Workspace().outdir()
        self._curr_subdir = ''
        self.trace = dict()
        self._update_version = kwargs.get('version_from_vcs', False)

        if self._method == 'guess':
            self._method = self._guess_method()
            dprint(f'Guessed method = {self._method}')

        # Fetch sources following the specified method and move them to the
        # temporary source build folder
        dprint('extracting sources in the temporary directory: {}'
               .format(self._builddir))
        self._create_srcdir(self._method)

    def __del__(self):
        # Skip removal if MMPACK_BUILD_KEEP_TMPDIR env is set to true
        try:
            if str2bool(os.environ.get('MMPACK_BUILD_KEEP_TMPDIR', 'false')):
                return
        except ValueError:
            pass

        # If source build dir has been created and not detach, remove it at
        # instance destruction
        dprint('Destroying temporary source build dir ' + self._builddir)
        rmtree_force(self._builddir)

    def _get_path_or_url_file(self) -> str:
        """
        Get path to file pointed to path_or_url. If not local file/path,
        the function will attempt to download the resource and return the path
        to downloaded_file if succeed
        """
        if self._downloaded_file:
            return self._downloaded_file

        if exists(self._path_url):
            dprint(f'{self._path_url} is a local file/dir')
            self._downloaded_file = self._path_url
            return self._downloaded_file

        path = join_path(self._builddir, basename(self._path_url))
        download(self._path_url, path)

        # If download fails, we will not reach here, hence _download_file will
        # not be set (as intended)
        dprint(f'remote {self._path_url} has been downloaded at {path}')
        self._downloaded_file = path

        return path

    def _guess_method(self) -> str:
        if _is_git_dir(self._path_url) or self._path_url.endswith('.git'):
            return 'git'
        elif os.path.isdir(self._path_url):
            return 'path'

        try:
            path = self._get_path_or_url_file()
            with taropen(path, 'r:*') as tar:
                if './mmpack/src_orig_tracing' in tar.getnames():
                    return 'srcpkg'
                else:
                    return 'tar'
        except (DownloadError, tarfile.ReadError):
            pass

        # If nothing has worked before, lets assume this is a git remote url
        return 'git'

    def _gen_project_sources(self, subdir: str = '') -> ProjectSource:
        srcdir = self._srcdir
        self._curr_subdir = subdir
        if subdir:
            self.trace['pkg']['subdir'] = subdir
            srcdir += '/' + subdir

        os.makedirs(self._get_prj_builddir(), exist_ok=True)

        # If the input was a mmpack source package, nothing to do besides copy
        # to output folder
        if self._method == 'srcpkg':
            srctar = f'{self._outdir}/{basename(self._path_url)}'
            name, version = get_name_version_from_srcdir(srcdir)
            try:
                shutil.copy(self._path_url, srctar)
            except shutil.SameFileError:
                pass
        else:
            self._process_source_strap(srcdir)
            self._store_src_orig_tracing(srcdir)

            # extract minimal metadata from package
            name, version = get_name_version_from_srcdir(srcdir)
            srctar = f'{self._outdir}/{name}_{version}_src.tar.xz'

            # Create source package tarball
            dprint('Building source tarball ' + srctar)
            create_tarball(srcdir, srctar, 'xz')

        return ProjectSource(name=name,
                             version=version,
                             tarball=srctar,
                             srcdir=srcdir)

    def _get_prj_builddir(self):
        return os.path.join(self._builddir, self._curr_subdir)

    def _get_unpacked_upstream_dir(self):
        """
        Get location when upstream source code is expected to be extracted if
        fetched. This does not created the folder.
        """
        return os.path.join(self._get_prj_builddir(), 'upstream')

    def _get_upstream_gitdir(self):
        return os.path.join(self._get_prj_builddir(), 'upstream.git')

    def _process_source_strap(self, srcdir):
        source_strap = os.path.join(srcdir, 'mmpack/sources-strap')
        try:
            specs = yaml_load(source_strap)
        except FileNotFoundError:
            return  # There is no source strap, nothing to be done

        # Execute create_srcdir build script if any
        env = {'PATH_URL': self._path_url}
        if self._vcsdir:
            env['VCSDIR'] = abspath(self._vcsdir)
        self._run_build_script('create_srcdir', self._method, srcdir, env)

        self._fetch_upstream(specs, srcdir)
        self._patch_sources(specs.get('patches', []), srcdir)

        # Run source strapped_hook
        self._run_build_script('source_strapped', specs['method'], srcdir)

    def _run_build_script(self, name: str, method: str,
                          execdir: str, env: Optional[Dict[str, str]] = None):
        if env is None:
            env = {}
        env['BUILDDIR'] = abspath(self._get_prj_builddir())

        specdir = join_path(self._srcdir, self._curr_subdir, 'mmpack')
        run_build_script(name, execdir, specdir, [method], env)

    def iter_mmpack_srcs(self) -> Iterator[ProjectSource]:
        """
        Get an iterator of sources of all project referenced in the repository
        """
        prjlist_path = self._srcdir + '/projects.mmpack'

        if exists(self._srcdir + '/mmpack/specs'):
            subdirs = ['']
        elif exists(prjlist_path):
            # List subdirs listed in projects.mmpack if any
            with open(prjlist_path, 'rt') as list_fp:
                subdirs = [p.strip() for p in list_fp]

            # Filter subdirs if requested in named argument build_only_modified
            only_modified = self._kwargs.get('build_only_modified', False)
            if only_modified and self._method == 'git':
                files = _git_modified_files(self._vcsdir)
                subdirs = [
                    d for d in subdirs
                    if any(map(lambda f, d=d: f.startswith(d + '/'), files))
                ]
        else:
            iprint(f'No mmpack source found in {self._srcdir}')
            return

        # iterate over project subdirs and generate the source package
        for subdir in subdirs:
            yield self._gen_project_sources(subdir)

    def _store_src_orig_tracing(self, srcdir: str):
        """
        Write src_orig_tracing file from trace information
        """
        upstream_info = self.trace.get('upstream', {'method': 'in-src-pkg'})
        data = {'packaging': self.trace['pkg'], 'upstream': upstream_info}

        patch_list = self.trace.get('patches')
        if patch_list:
            data['patches'] = patch_list

        file_path = os.path.join(srcdir, 'mmpack/src_orig_tracing')
        yaml_serialize(data, file_path, use_block_style=True)

    def _update_version_from_git(self):
        """
        rewrite the version of the project using git describe. This will work
        and is meaningful only if there is a mmapck/specs at the root directory
        of project, ie, it is not a multi project.
        """
        # Try load the specs of the project and describe from the tag named
        # after the version in the specs. In case of no packaging or multi
        # project, this will fail. Just return in that case.
        try:
            specs_path = os.path.join(self._srcdir, 'mmpack/specs')
            tag = specs_load(specs_path)['version']
            version = _git_subcmd(['describe', '--match', tag], self._vcsdir)

            # read specs as binary blob
            with open(specs_path, 'r', encoding='utf-8') as stream:
                specs = stream.read()

            pat = re.compile(r'(\s*version\s*:\s*)[^\s,}]+')
            updated_specs = pat.sub(lambda m: m.groups()[0] + version, specs)

            # Overwrite the spec file with updated specs
            with open(specs_path, 'w', encoding='utf-8') as stream:
                stream.write(updated_specs)
        except (FileNotFoundError, ShellException):
            pass

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

        if self._update_version:
            self._update_version_from_git()

        commit_ref = _git_subcmd(['rev-parse', 'HEAD'], self._vcsdir)
        self.trace['pkg'].update({'url': self._path_url, 'ref': commit_ref})

    def _create_srcdir_from_tar(self):
        """
        Create a source package folder from tarball, not necessarily named
        following mmpack src tarball convention. If tarball is not local,
        download it.
        """
        path = self._get_path_or_url_file()

        with taropen(path, 'r:*') as tar:
            if ('mmpack/specs' in tar.getnames()
                    or 'mmpack.projects' in tar.getnames()):
                members = None
            else:
                members = _strip_leading_comp_tar_iter(tar)

            tar.extractall(path=self._srcdir, members=members)

        # Get tag name if not set yet
        if not self.tag:
            self.tag = 'from_tar'

        self.trace['pkg'].update({'filename': self._path_url,
                                  'sha256': sha256sum(path)})

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
        gitdir = self._get_upstream_gitdir()
        url = specs['url']

        os.makedirs(srcdir, exist_ok=True)

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
        downloaded_file = os.path.join(self._get_prj_builddir(), filename)
        expected_sha256 = specs.get('sha256')

        cached_download(url, downloaded_file, expected_sha256)

        # Verify sha256 is correct if supplied in specs
        file_hash = sha256sum(downloaded_file)
        if expected_sha256 and expected_sha256 != file_hash:
            raise Assert("Downloaded file does not match expected sha256")

        self.trace['upstream'].update({'url': url, 'sha256': file_hash})

        # Extract downloaded_file in upstreamdir
        with taropen(downloaded_file, 'r:*') as tar:
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

        upstream_srcdir = self._get_unpacked_upstream_dir()

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

        # Run fetch_upstream_hook
        env = {'URL': specs['url']}
        if method == 'git':
            env['VCSDIR'] = abspath(self._get_upstream_gitdir())
        self._run_build_script('fetch_upstream', method, upstream_srcdir, env)

        # Move extracted upstream sources except mmpack packaging
        for elt in os.listdir(upstream_srcdir):
            if elt != 'mmpack':
                shutil.move(os.path.join(upstream_srcdir, elt), srcdir)

    def _patch_sources(self, patches: List[str], srcdir: str):
        for patch in patches:
            iprint(f'Applying {patch}...')
            with open(os.path.join(srcdir, patch), 'rb') as patchfile:
                run_cmd(['patch', '-d', srcdir, '-p1'], stdin=patchfile)

        self.trace['patches'] = patches
