#!/usr/bin/env python3
'''
TODOC
'''

import os
try:
    home = os.environ['HOME']
except Exception:
    raise  # TODO: windows ?

HOME = home
CONFIG_PATH = home + "/.mmpack-config.yml"
MMPACK_DB_ROOT = "/var/lib/mmpack/"
