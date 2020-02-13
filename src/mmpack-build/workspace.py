# @mindmaze_header@
"""
os helpers to manipulate the paths and environments
"""

import os
import shutil
import tempfile

from . common import shell, dprint, ShellException, pushdir, popdir, \
    download, sha256sum, iprint
from . decorators import singleton
from . settings import BINDIR, EXEEXT
from . xdg import XDG_CONFIG_HOME, XDG_CACHE_HOME, XDG_DATA_HOME


def find_project_root_folder() -> str:
    """
    Look for folder named 'mmpack' in the current directory or any parent
    folder.
    """
    pwd = os.getcwd()
    if os.path.isdir('mmpack') and os.path.isfile('mmpack/specs'):
        return pwd

    parent, current = os.path.split(pwd)
    if not current:
        return None
    pushdir(parent)
    root_folder = find_project_root_folder()
    popdir()
    return root_folder


@singleton
class Workspace:
    # pylint: disable=too-many-instance-attributes
    """
    global mmpack workspace singleton class
    """

    def __init__(self):
        self.config = XDG_CONFIG_HOME + '/mmpack-config.yaml'
        self.sources = XDG_CACHE_HOME + '/mmpack/sources'
        self.build = XDG_CACHE_HOME + '/mmpack/build'
        self.packages = XDG_DATA_HOME + '/mmpack-packages'
        self.cache = XDG_CACHE_HOME + '/mmpack/cache'
        self.tmp = tempfile.mktemp(dir=self.cache)
        self._cygpath_root = None
        self._mmpack_bin = None
        self.prefix = ''

        # create the directories if they do not exist
        os.makedirs(XDG_CONFIG_HOME, exist_ok=True)
        os.makedirs(self.build, exist_ok=True)
        os.makedirs(self.sources, exist_ok=True)
        os.makedirs(self.packages, exist_ok=True)
        os.makedirs(self.cache, exist_ok=True)

    def cygroot(self) -> str:
        """
        under msys, returns stripped output of: cygpath -w '/'
        under linux, returns an empty string
        """
        if self._cygpath_root is not None:
            return self._cygpath_root

        try:
            self._cygpath_root = shell(['cygpath', '-w', '/'], log=False)
            self._cygpath_root = self._cygpath_root.strip()
        except ShellException:
            self._cygpath_root = ''

        return self._cygpath_root

    def mmpack_bin(self) -> str:
        """
        Get mmpack binary absolute path (maybe transformed with cygpath)
        """
        if self._mmpack_bin is not None:
            return self._mmpack_bin

        mmpack_bin = BINDIR + '/mmpack' + EXEEXT
        try:
            self._mmpack_bin = shell(['cygpath', '-w', mmpack_bin], log=False)
            self._mmpack_bin = self._mmpack_bin.strip()
        except ShellException:
            self._mmpack_bin = mmpack_bin

        return self._mmpack_bin

    def builddir(self, srcpkg: str, tag: str):
        """
        get package build directory. Create it if needed.
        """
        builddir = self.build + '/' + srcpkg + '/' + tag
        os.makedirs(builddir, exist_ok=True)
        return builddir

    def srcclean(self, srcpkg: str = ''):
        """
        remove all copied sources.
        if pkg is explicit, will only clean given package
        """
        if srcpkg:
            dprint('cleaning {0} sources'.format(srcpkg))

        shell('rm -rvf {0}/{1}*'.format(self.sources, srcpkg))

    def clean(self, srcpkg: str = '', tag: str = ''):
        """
        remove all temporary build objects keep generated packages. if
        srcpkg is explicit, will only clean given package. If tag is further
        explicted, only tag specific subfolder of srcpkg is cleaned.
        """
        dprint('cleaning {0} workspace'.format(srcpkg + '/' + tag))
        shell('rm -rvf {0}/{1}/{2}'.format(self.build, srcpkg, tag))

    def wipe(self):
        """
        clean sources, build, staging, and all packages
        """
        self.srcclean()
        self.clean()
        shell('rm -vrf {0}/* {1}/*'.format(self.packages, self.cache))

    def cache_get(self, path: str, expected_sha256: str = None) -> bool:
        """
        Get file from cache if a copy is present

        Args:
            path: path of the file to copy (lookup done on basename)
            expected_sha256: if not None, expected sha256 of the file.

        Return: True if the a cached version has been copied, False otherwise
        """
        cache_file = os.path.join(self.cache, os.path.basename(path))
        try:
            if not expected_sha256 or expected_sha256 == sha256sum(cache_file):
                shutil.copyfile(cache_file, path)
                return True
        except FileNotFoundError:
            pass

        return False

    def cache_file(self, path: str):
        """
        Copy file into cache
        """
        cache_file = os.path.join(self.cache, os.path.basename(path))
        shutil.copyfile(path, cache_file)


def get_local_install_dir(builddir: str):
    """
    Get install dir in src package building path
    """
    instdir = builddir + '/local-install'
    os.makedirs(instdir, exist_ok=True)
    return instdir


def get_staging_dir(builddir: str, binpkg_name: str):
    """
    Get package staging dir in src package building path
    """
    stagedir = '{0}/staging/{1}'.format(builddir, binpkg_name)
    os.makedirs(stagedir, exist_ok=True)
    return stagedir


def is_valid_prefix(prefix: str) -> bool:
    """
    returns whether given prefix is a valid path for mmpack prefix
    """
    return os.path.exists(prefix + '/var/lib/mmpack/')


def cached_download(url: str, path: str, expected_sha256: str = None):
    """
    Download file from url or copy from cache available to the specified path.

    Args:
        url: URL of the resource to download
        path: path where to save downloaded file
        expected_sha256: if not None, expected sha256 of the file to download.
            The file will be redownloaded if sha256 of cached file does not
            match.
    """
    wrk = Workspace()

    if wrk.cache_get(path, expected_sha256):
        iprint('Skip download {}. Using cached file'.format(url))
        return

    download(url, path)
    wrk.cache_file(path)
