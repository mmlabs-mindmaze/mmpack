# @mindmaze_header@
"""
plugin tracking containing python file handling functions
"""

import filecmp
import os
import re
import shutil
from glob import glob
from typing import Set, Dict

from base_hook import BaseHook


# example of matches:
# 'lib/python3.6/site-packages/foo.so' => foo
# 'lib/python3/site-packages/_foo.so' => foo
# 'lib/python3/site-packages/foo_bar.py' => foo_bar
# 'lib/python3/site-packages/foo/__init__.py' => foo
# 'lib/python3/site-packages/foo/_internal.so' => foo
# 'lib/python2/site-packages/foo.so' => None
_PKG_REGEX = re.compile(r'lib/python3(?:\.\d)?/site-packages/_?([\w_]+)')


def _get_python3_pkgname(filename: str) -> str:
    """
    Return the mmpack package a file should belong to.
    """
    res = _PKG_REGEX.search(filename)
    if not res:
        return None

    pypkg_name = res.groups()[0]
    return 'python3-' + pypkg_name.lower()


class MMPackBuildHook(BaseHook):
    """
    Hook tracking python module used and exposed
    """

    def post_local_install(self):
        """
        install move python3 packages from versioned python3 install folder
        to a unversioned python3 folder. This way, python3 version can be
        upgraded (normally, only python3 standard library has to be
        installed in a version installed folder).
        """
        # Move all public package from unversioned python3 folder to an
        # unversioned one
        for pydir in glob('lib/python3.*/site-packages'):
            os.makedirs('lib/python3/site-packages', exist_ok=True)
            for srcdir, dirs, files in os.walk(pydir):
                reldir = os.path.relpath(srcdir, pydir)
                dstdir = os.path.join('lib/python3/site-packages', reldir)

                # Create the folders in unversioned python3 site package
                for dirpath in dirs:
                    os.makedirs(os.path.join(dstdir, dirpath), exist_ok=True)

                # Move files to unversioned python3 site package
                for filename in files:
                    src = os.path.join(srcdir, filename)
                    dst = os.path.join(dstdir, filename)

                    # If destination file exists, we have no issue if source
                    # and destination are the same
                    if os.path.lexists(dst):
                        if filecmp.cmp(src, dst):
                            continue
                        raise FileExistsError

                    os.replace(src, dst)

            # Remove the remainings
            shutil.rmtree(pydir)

    def get_dispatch(self, install_files: Set[str]) -> Dict[str, Set[str]]:
        pkgs = dict()
        for file in install_files:
            pkgname = _get_python3_pkgname(file)
            if pkgname:
                pkgfiles = pkgs.setdefault(pkgname, set())
                pkgfiles.add(file)

        return pkgs
