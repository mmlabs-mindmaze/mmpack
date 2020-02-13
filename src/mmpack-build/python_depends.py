#!/usr/bin/env python3
# @mindmaze_header@
"""
python depends extractor.

Given a set of files that belong to the same mmpack package, this utility will
scan the python modules and register the symbol used from other modules out of
the mmpack package.

It will print on standard output the qualified name of the public symbols used.

LIMITATION:
Currently the inference performed on imported symbol name go too deep and the
qualified name refer to the one as define internally in the imported module,
not the name as declared for example in the __init__.py.
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath
from typing import Set, Iterator

from astroid import MANAGER as astroid_manager
from astroid import Uninferable, FunctionDef
from astroid.exceptions import InferenceError, NameInferenceError, \
    AttributeInferenceError
from astroid.modutils import is_standard_module
from astroid.node_classes import Call, Attribute
from mmpack_build.file_utils import is_python_script


def _is_module_packaged(mod, pkgfiles: Set[str]) -> bool:
    """
    test whether a specified module is provided by a file of the same
    mmpack package.
    """
    return mod.path and mod.path[0] in pkgfiles


def _inspect_load_entry_point(call: Call):
    """
    Return the __main__ symbol of loaded module
    """
    modreq = next(call.args[0].infer()).value
    modname = modreq.split('==')[0]
    return modname + '.__main__'


def _call_func_iter(call: Call) -> Iterator[FunctionDef]:
    """
    equivalent to call.func.infer() supporting recent version of astroid
    """
    if isinstance(call.func, Attribute):
        attr = call.func
        for base in attr.expr.infer():
            for funcdef in base.getattr(attr.attrname):
                yield funcdef
    else:
        for funcdef in call.func.infer():
            yield funcdef


def _inspect_call(call: Call, used_symbols: Set[str], pkgfiles: Set[str]):
    try:
        for funcdef in _call_func_iter(call):
            orig_mod = funcdef.root()

            # ignore inferred call that does not generate dependencies
            # (uninferable or call to stdlib or internal call)
            if (orig_mod == Uninferable
                    or is_standard_module(orig_mod.name)
                    or _is_module_packaged(orig_mod, pkgfiles)):
                continue

            qname = funcdef.qname()
            if qname == 'pkg_resources.load_entry_point':
                used_symbols.add(_inspect_load_entry_point(call))

            used_symbols.add(qname)

    # As python is a dynamic language, uninferable name lookup or uninferable
    # object can be common (when it highly depends on the context that we
    # cannot have without executing the code) Hence, it is safer to ignore.
    except (NameInferenceError, InferenceError, AttributeInferenceError):
        pass


def _inspect_node(node, used_symbols: Set[str], pkgfiles: Set[str]):
    if isinstance(node, Call):
        _inspect_call(node, used_symbols, pkgfiles)

    for child in node.get_children():
        _inspect_node(child, used_symbols, pkgfiles)


def _gen_py_depends(filename: str, pkgfiles: Set[str]) -> Set[str]:
    """
    Generate the set of qualified named of used symbols of a python module and
    imported from modules not in the same mmpack package.

    Args:
        filename: absolute path of a python module
        pkgfiles: set of absolute path of files in the same mmpack packaged

    Returns: Set of qualified name of imported symbols
    """
    tree = astroid_manager.ast_from_file(filename)

    used_symbols = set()
    for node in tree.body:
        _inspect_node(node, used_symbols, pkgfiles)

    return used_symbols


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--site-path', dest='site_path', type=str, nargs='?',
                        help='path of python site-packages or folder '
                        'containing python package')

    return parser.parse_args()


def main():
    """
    python_depends utility entry point
    """
    options = parse_options()

    # If site path folder is specified, add it to sys.path so astroid resolve
    # the imports properly
    if options.site_path:
        sys.path.insert(0, abspath(options.site_path))

    pkgfiles = [abspath(f.strip()) for f in sys.stdin.readlines()]
    pyfiles = [f for f in pkgfiles if is_python_script(f)]

    symbol_set = set()
    for filename in pyfiles:
        symbol_set.update(_gen_py_depends(filename, pkgfiles))

    # Return sorted results on stdout
    symlist = list(symbol_set)
    symlist.sort()
    for sym in symlist:
        print(sym)


if __name__ == '__main__':
    main()
