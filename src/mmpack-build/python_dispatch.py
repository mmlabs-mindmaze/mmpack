#!/usr/bin/env python3
# @mindmaze_header@
"""
python public package dispatch
"""

import json
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath
from typing import Dict, List, Set

from astroid import MANAGER
from astroid.modutils import (file_info_from_modpath, is_namespace,
                              modpath_from_file_with_callback)


_IS_NS: Dict[str, bool] = {}


def _is_namespace_mod(modpath: List[str]) -> bool:
    modname = '.'.join(modpath)
    is_ns = _IS_NS.get(modname)
    if is_ns is not None:
        return is_ns

    try:
        is_ns = is_namespace(file_info_from_modpath(modpath))
    except ImportError:
        is_ns = False
    _IS_NS[modname] = is_ns
    return is_ns


def _get_root_modname(pyfile: str) -> str:
    modpath = modpath_from_file_with_callback(abspath(pyfile), None,
                                              lambda x, y: True)

    # Find first parent package that is not a namespace
    for depth in range(1, len(modpath)+1):
        root_modpath = modpath[:depth]
        if not _is_namespace_mod(root_modpath):
            break

    return '.'.join(root_modpath)


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--site-path', dest='site_paths', type=str, nargs='?',
                        action='append', default=[],
                        help='path of python site-packages or folder '
                        'containing python package')
    parser.add_argument('infiles', type=str, nargs='*')

    return parser.parse_args()


def main():
    """
    python_dispatch utility entry point
    """
    options = parse_options()

    MANAGER.always_load_extensions = True

    # If site path folders are specified, add them to sys.path so that astroid
    # resolve the imports properly
    for sitedir in options.site_paths:
        sys.path.insert(0, abspath(sitedir))

    pypkgs: Dict[str, Set[str]] = {}
    for pyfile in options.infiles:
        modname = _get_root_modname(pyfile)
        pypkgs.setdefault(modname, set()).add(pyfile)

    # Return results as JSON dict on stdout
    json.dump({k: list(sorted(v)) for k, v in pypkgs.items()},
              fp=sys.stdout, indent='    ', separators=(',', ': '))

if __name__ == '__main__':
    main()
