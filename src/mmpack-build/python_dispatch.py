#!/usr/bin/env python3
# @mindmaze_header@
"""
python public package dispatch
"""

import json
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath, basename
from platform import system
from sysconfig import get_config_var
from typing import Dict, List, Set

from astroid import MANAGER
from astroid.modutils import (file_info_from_modpath, is_namespace,
                              modpath_from_file_with_callback)


IS_NS: Dict[str, bool] = {}


LIMITED_API_EXTENSION_SUFFIX = '.abi3.so'
if system() == 'Windows':
    LIMITED_API_EXTENSION_SUFFIX = '.pyd'


COMPILED_EXT_SUFFIXES = (
    get_config_var('EXT_SUFFIX'),
    LIMITED_API_EXTENSION_SUFFIX,
)


def is_namespace_mod(modpath: List[str]) -> bool:
    """reports true if modpath is a PEP420 namespace package"""
    modname = '.'.join(modpath)
    is_ns = IS_NS.get(modname)
    if is_ns is not None:
        return is_ns

    try:
        is_ns = is_namespace(file_info_from_modpath(modpath))
    except ImportError:
        is_ns = False
    IS_NS[modname] = is_ns
    return is_ns


def get_root_modname(pyfile: str) -> str:
    """Get the deepest regular package"""
    modpath = modpath_from_file_with_callback(abspath(pyfile), None,
                                              lambda x, y: True)

    # Fix module name of compiled module path: they are badly inferred from
    # their filename
    for suffix in COMPILED_EXT_SUFFIXES:
        if pyfile.endswith(suffix):
            modpath[-1] = basename(pyfile)[:-len(suffix)]
            break

    # Find first parent package that is not a namespace
    for depth in range(1, len(modpath)+1):
        root_modpath = modpath[:depth]
        if not is_namespace_mod(root_modpath):
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
        modname = get_root_modname(pyfile)
        pypkgs.setdefault(modname, set()).add(pyfile)

    # Return results as JSON dict on stdout
    json.dump({k: list(sorted(v)) for k, v in pypkgs.items()},
              fp=sys.stdout, indent='    ', separators=(',', ': '))


if __name__ == '__main__':
    main()
