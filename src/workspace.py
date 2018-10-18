# @mindmaze_header@
'''
os helpers to manipulate the paths and environments
'''

import os
import sys
from xdg import XDG_CONFIG_HOME, XDG_CACHE_HOME, XDG_DATA_HOME

from decorators import singleton
from common import shell, dprint, ShellException


@singleton
class Workspace(object):
    'global mmpack workspace singleton class'
    def __init__(self):
        self.config = XDG_CONFIG_HOME + '/mmpack-config.yml'
        self.sources = XDG_CACHE_HOME + '/mmpack-sources'
        self.build = XDG_CACHE_HOME + '/mmpack-build'
        self.staging = XDG_CACHE_HOME + '/mmpack-staging'
        self.packages = XDG_DATA_HOME + '/mmpack-packages'
        self._cygpath_root = None
        self.prefix = ''

        # create the directories if they do not exist
        os.makedirs(XDG_CONFIG_HOME, exist_ok=True)
        os.makedirs(self.build, exist_ok=True)
        os.makedirs(self.sources, exist_ok=True)
        os.makedirs(self.packages, exist_ok=True)

    def cygroot(self) -> str:
        '''under msys, returns stripped output of: cygpath -w '/'
           under linux, returns an empty string
        '''
        if self._cygpath_root is not None:
            return self._cygpath_root

        try:
            self._cygpath_root = shell("cygpath -w '/' ").strip()
        except ShellException:
            self._cygpath_root = ''

        return self._cygpath_root

    def builddir(self, srcpkg: str):
        'get package build directory. Create it if needed.'

        builddir = self.build + '/' + srcpkg
        os.makedirs(builddir, exist_ok=True)
        return builddir

    def stagedir(self, binpkg: str):
        ''' Get package staging directory (create if needed).

        Following package local-install, this is where the files will get
        moved to just before package creation.
        '''
        stagingdir = self.staging + '/' + binpkg
        os.makedirs(stagingdir, exist_ok=True)
        return stagingdir

    def srcclean(self, srcpkg: str=''):
        '''remove all copied sources
           if pkg is explicited, will only clean given package
        '''
        if srcpkg:
            dprint('cleaning {0} sources'.format(srcpkg))

        shell('rm -rvf {0}/{1}*'.format(self.sources, srcpkg))

    def clean(self, srcpkg: str=''):
        '''remove all temporary build objects keep generated packages.
           if pkg is explicited, will only clean given package
        '''
        if srcpkg:
            dprint('cleaning {0} workspace'.format(srcpkg))

        shell('rm -rvf {0}/{1}*'.format(self.staging, srcpkg))
        shell('rm -rvf {0}/{1}*'.format(self.build, srcpkg))

        # remove all possibly create library packages.
        # assume they contain the srcpkg name within them
        shell('rm -rvf {0}/lib*{1}*'.format(self.staging, srcpkg))
        shell('rm -rvf {0}/lib*{1}*'.format(self.build, srcpkg))

    def wipe(self):
        'clean sources, build, staging, and all packages'
        self.srcclean()
        self.clean()
        shell('rm -vf {0}/*'.format(self.packages))


def is_valid_prefix(prefix: str) -> bool:
    'returns whether given prefix is a valid path for mmpack prefix'
    return os.path.exists(prefix + '/var/lib/mmpack/')


def push_prefix(prefix: str):
    ''' Prepend given prefix to all relevant os environment variables
        Since python is launched in its own process, directly write to
        os.environ.
    '''
    # system environment execution
    os.environ['PATH'] = '{0}/bin:{1}'.format(prefix, os.environ.get('PATH'))
    os.environ['LD_LIBRARY_PATH'] = '{0}/lib:{1}' \
                                    .format(prefix,
                                            os.environ.get('LD_LIBRARY_PATH'))
    os.environ['LIBRARY_PATH'] = '{0}/lib:{1}' \
                                 .format(prefix,
                                         os.environ.get('LIBRARY_PATH'))
    os.environ['CPATH'] = '{0}/include:{1}' \
                          .format(prefix, os.environ.get('CPATH'))

    pythonpath = '/lib/python/{:d}.{:d}'.format(sys.version_info.major,
                                                sys.version_info.minor)
    os.environ['PYTHONPATH'] = '{0}/{1}:{2}' \
                               .format(prefix, pythonpath,
                                       os.environ.get('PYTHONPATH'))
