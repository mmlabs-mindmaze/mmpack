#!/usr/bin/env python3
'''
TODOC
'''

import os

try:
    CFG_HOME = os.environ['XDG_CONFIG_HOME']
except KeyError:
    CFG_HOME = os.environ['HOME'] + '/.config'

CONFIG_PATH = CFG_HOME + "/mmpack-config.yml"
MMPACK_DB_ROOT = "/var/lib/mmpack/"
