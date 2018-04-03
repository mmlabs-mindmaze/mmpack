#!/usr/bin/env python3
'''
TODOC
'''

import os
import glob
import re
import yaml

from typing import List
from settings import MMPACK_DB_ROOT


class mm_pkg_db(object):
    '''
    ***all files are in yaml format***
    /var/lib/mmpack/
    ├── lock  # global write lock
    ├── installed
    │   └── package-name
    │       ├── package-name.info
    │       ├── package-name.post-install
    │       ├── package-name.list
    │       ├── package-name.sha1
    │       ├── package-name.symbols
    │       └── package-name.pre-uninstall
    └── remotes
        ├── mmpack-server-1
        │   ├── pkg-1 / ...
        │   └── pkg-2 / ...
        └── mmpack-server-2
    '''
    def __init__(self, db_root: str = MMPACK_DB_ROOT):
        '''
        allow overriding db_root for easier tests
        '''
        self.db_root = db_root

    def list_installed(self):
        'list installed packages'
        for file in os.listdir('{0}/{1}'.format(self.db_root, 'installed')):
            print(package-name)

    def pkg_provides(self, file_regex: str):
        '''
        prints a list of packages that could provide <file_regex>
        file_regex is a pcre expression
          it will be prefixed and posfixed with ".*"
        '''
        regex = '{0}{1}{0}'.format('.*', file_regex)
        for listfile in glob.glob('{0}/*/*.list'.format(self.db_root)):
            with open(listfile, 'r') as text:
                if re.findall(regex, text.read()):
                    print(listfile)
