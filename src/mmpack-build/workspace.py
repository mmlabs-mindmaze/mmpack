# @mindmaze_header@
"""
os helpers to manipulate the paths and environments
"""

import os
import shutil
from datetime import datetime, timedelta
from tempfile import mkdtemp
from time import time_ns

from .common import shell, dprint, download, sha256sum, iprint, rmfile
from .decorators import singleton
from .errors import ShellException
from .settings import BINDIR, EXEEXT
from .xdg import XDG_CACHE_HOME, XDG_DATA_HOME


def find_project_root_folder(find_multiproj: bool = False) -> str:
    """
    Look for folder named 'mmpack' in the current directory or any parent
    folder.
    """
    path = os.getcwd()

    while True:
        if os.path.isfile(path + '/mmpack/specs'):
            break

        if find_multiproj and os.path.isfile(path + '/projects.mmpack'):
            break

        path, current = os.path.split(path)
        if not current:
            return None

    return path


def _get_prefix_abspath(prefix: str):
    if not ((os.sep in prefix)
            or (os.altsep is not None and os.altsep in prefix)):
        return XDG_DATA_HOME + '/mmpack/prefix/' + prefix

    return os.path.abspath(prefix)


@singleton
class Workspace:
    # pylint: disable=too-many-instance-attributes
    """
    global mmpack workspace singleton class
    """

    def __init__(self):
        self._build = os.environ.get('MMPACK_BUILD_BUILDDIR',
                                     XDG_CACHE_HOME + '/mmpack/build')
        self._packages = os.environ.get('MMPACK_BUILD_OUTDIR',
                                        XDG_DATA_HOME + '/mmpack/packages')
        self._cache = os.environ.get('MMPACK_BUILD_CACHEDIR',
                                     XDG_CACHE_HOME + '/mmpack/cache')
        self._cygpath_root = None
        self._mmpack_bin = None
        self.prefix = ''

        prefix = os.environ.get('MMPACK_PREFIX')
        if prefix:
            self.prefix = _get_prefix_abspath(prefix)

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

    def set_outdir(self, path: str):
        """
        set package output directory.
        """
        self._packages = path

    def set_builddir(self, path: str):
        """
        set build directory.
        """
        self._build = path

    def set_cachedir(self, path: str):
        """
        set cache directory.
        """
        self._cache = path

    def tmpdir(self):
        """
        Get a temporary folder in mmpack build dir
        """
        os.makedirs(self._build, exist_ok=True)
        tmpdir = mkdtemp(dir=self._build)
        return tmpdir

    def outdir(self):
        """
        get package output directory. Create it if needed.
        """
        os.makedirs(self._packages, exist_ok=True)
        return self._packages

    def builddir(self, srcpkg: str, tag: str):
        """
        get package build directory. Create it if needed.
        """
        builddir = self._build + '/' + srcpkg + '/' + tag
        os.makedirs(builddir, exist_ok=True)
        return builddir

    def clean(self, srcpkg: str = '', tag: str = ''):
        """
        remove all temporary build objects keep generated packages. if
        srcpkg is explicit, will only clean given package. If tag is further
        explicted, only tag specific subfolder of srcpkg is cleaned.
        """
        dprint('cleaning {0} workspace'.format(srcpkg + '/' + tag))
        shell('rm -rvf {0}/{1}/{2}'.format(self._build, srcpkg, tag))

    def wipe(self):
        """
        clean sources, build, staging, and all packages
        """
        self.clean()
        shell('rm -vrf {0}/* {1}/*'.format(self._packages, self._cache))

    def cache_get(self, path: str, expected_sha256: str = None) -> bool:
        """
        Get file from cache if a copy is present

        Args:
            path: path of the file to copy (lookup done on basename)
            expected_sha256: if not None, expected sha256 of the file.

        Return: True if the a cached version has been copied, False otherwise
        """
        if expected_sha256:
            cache_file = os.path.join(self._cache, expected_sha256)
        else:
            cache_file = os.path.join(self._cache, os.path.basename(path))

        try:
            shutil.copyfile(cache_file, path)
            return True
        except FileNotFoundError:
            pass

        return False

    def cache_file(self, path: str):
        """
        Copy file into cache
        """
        os.makedirs(self._cache, exist_ok=True)
        cache_file = os.path.join(self._cache, os.path.basename(path))
        rmfile(cache_file)
        shutil.copyfile(path, cache_file)

        # Hard link a version named after the sha256 value of the cached file
        os.link(cache_file, os.path.join(self._cache, sha256sum(cache_file)))

    def cleanup_cache(self):
        """
        remove from cache files that have not been accessed in a while
        """
        outdated_time = (datetime.now() - timedelta(days=7)).timestamp()
        rmlist = set()
        try:
            with os.scandir(self._cache) as dir_it:
                rmlist = {e.path for e in dir_it
                          if e.is_file() and e.stat().st_atime < outdated_time}
        except FileNotFoundError:
            return

        for path in rmlist:
            rmfile(path)

    def set_prefix(self, prefix: str):
        """
        Configure workspace to use specified prefix when building
        """
        self.prefix = _get_prefix_abspath(prefix)


def is_valid_prefix(prefix: str) -> bool:
    """
    returns whether given prefix is a valid path for mmpack prefix
    """
    return os.path.exists(prefix + '/var/lib/mmpack/')


def _update_access_time(path: str):
    mtime_ns = os.stat(path).st_mtime_ns
    curtime_ns = time_ns()
    os.utime(path, ns=(curtime_ns, mtime_ns))


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
        _update_access_time(path)
        return

    download(url, path)
    wrk.cache_file(path)
