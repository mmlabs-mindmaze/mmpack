#!/usr/bin/python3

import json
import os
import shutil
import sys

destdir = sys.argv[1]
manifest_file = sys.argv[2]


def verbose(msg):
    if 'V' in os.environ:
        print(msg)


with open(manifest_file) as f:
    manifest = json.load(f)


if sys.platform == 'linux':
    def destdir_path(dst):
        if dst.startswith('/'):
            dst = dst[1:]
        return os.path.join(destdir, dst)
else:
    def destdir_path(dst):
        dst = os.popen('cygpath -u ' + dst).read()
        dst = dst[1:].strip()  # remove leading '/', trailing '\n' and blanks
        return os.path.join(destdir, dst)


for src, dst in manifest.items():
    # skip documentation: no use, and messes up target dependencies
    if 'doc/' in dst or 'man/' in dst :
        continue
    dst = destdir_path(dst)
    verbose("'{}' -> '{}'".format(src, dst))
    if os.path.isdir(src):
        os.makedirs(dst, exist_ok=True)
    else:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst, follow_symlinks=True)
