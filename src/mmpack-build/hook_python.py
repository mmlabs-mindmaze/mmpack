# @mindmaze_header@
"""
plugin tracking containing python file handling functions
"""

import re
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

    def get_dispatch(self, install_files: Set[str]) -> Dict[str, Set[str]]:
        pkgs = dict()
        for file in install_files:
            pkgname = _get_python3_pkgname(file)
            if pkgname:
                pkgfiles = pkgs.setdefault(pkgname, set())
                pkgfiles.add(file)

        return pkgs
