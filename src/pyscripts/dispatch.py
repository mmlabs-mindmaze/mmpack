#!/usr/bin/env python3
# @mindmaze_header@
"""
python public package dispatch
"""

import json
import sys
from os.path import abspath, basename
from platform import system
from sysconfig import get_config_var
from typing import Dict, Iterable, Set

from astroid.modutils import modpath_from_file_with_callback

from .utils import is_namespace_pkg


LIMITED_API_EXTENSION_SUFFIX = '.abi3.so'
if system() == 'Windows':
    LIMITED_API_EXTENSION_SUFFIX = '.pyd'


COMPILED_EXT_SUFFIXES = (
    get_config_var('EXT_SUFFIX'),
    LIMITED_API_EXTENSION_SUFFIX,
)


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
        root_modname = '.'.join(modpath[:depth])
        if not is_namespace_pkg(root_modname):
            break

    return root_modname


def run_dispatch(input_files: Iterable[str]):
    """
    python_dispatch utility entry point
    """
    pypkgs: Dict[str, Set[str]] = {}
    for pyfile in input_files:
        modname = get_root_modname(pyfile)
        pypkgs.setdefault(modname, set()).add(pyfile)

    # Return results as JSON dict on stdout
    json.dump({k: list(sorted(v)) for k, v in pypkgs.items()},
              fp=sys.stdout, indent='    ', separators=(',', ': '))
