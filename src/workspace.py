#!/usr/bin/env python3
'''
TODOC
'''

import os

from singleton_decorator import singleton
from common import shell
from settings import HOME


@singleton
class Workspace(object):
    'global mmpack workspace singleton class'
    def __init__(self, root=None):
        if not root:
            self.root = HOME + '/.mmpack'

        self.build = self.root + '/build'
        self.sources = self.root + '/sources'
        self.packages = self.root + '/packages'

        os.makedirs(self.root, exist_ok=True)
        os.makedirs(self.build, exist_ok=True)
        os.makedirs(self.sources, exist_ok=True)
        os.makedirs(self.packages, exist_ok=True)

    def builddir(self, pkg):
        'get package build directory. Create it if needed.'

        builddir = self.build + '/' + pkg
        os.makedirs(builddir, exist_ok=True)
        return builddir

    def clean(self, pkg=None):
        '''remove all copied sources and temporary build objects
           keep generated packages.
           if pkg is explicited, will only clean given package
        '''
        if not pkg:
            pkg = ''

        shell('rm -rvf {0}/{1}*'.format(self.build, pkg))
        shell('rm -rvf {0}/{1}*'.format(self.sources, pkg))
        shell('rm -rvf {0}/{1}*'.format(self.packages, pkg))

    def wipe(self):
        'same as clean, but also remove all created pacakges'
        self.clean()
        shell('rm -vf {0}/*'.format(self.packages))
