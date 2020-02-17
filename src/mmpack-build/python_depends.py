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
from typing import Set, Iterator, Tuple

from astroid import MANAGER as astroid_manager
from astroid.bases import Instance
from astroid.exceptions import InferenceError, NameInferenceError, \
    AttributeInferenceError
from astroid.modutils import is_standard_module
from astroid.node_classes import Call, Attribute, Name, \
    Import, ImportFrom, NodeNG
from mmpack_build.file_utils import is_python_script


def _is_module_packaged(mod, pkgfiles: Set[str]) -> bool:
    """
    test whether a specified module is provided by a file of the same
    mmpack package.
    """
    return mod.path and mod.path[0] in pkgfiles


def _is_external_pkg(mod, pkgfiles: Set[str]) -> bool:
    return not (_is_module_packaged(mod, pkgfiles)
                or is_standard_module(mod.name))


def _inspect_load_entry_point(call: Call):
    """
    Return the __main__ symbol of loaded module
    """
    modreq = next(call.args[0].infer()).value
    modname = modreq.split('==')[0]
    return modname + '.__main__'


def _infer_node_def(node: NodeNG) -> Iterator[NodeNG]:
    """
    equivalent to node.infer() supporting recent version of astroid
    """
    if isinstance(node, Attribute):
        attr = node
        for base in attr.expr.infer():
            for nodedef in base.igetattr(attr.attrname):
                yield nodedef
    else:
        for nodedef in node.infer():
            yield nodedef


def _get_module_name(imp: Import, name: str,
                     pkgfiles: Set[str]) -> Tuple[str, bool]:
    name = imp.real_name(name)
    module = imp.do_import_module(name)
    is_external = _is_external_pkg(module, pkgfiles)

    return (name, is_external)


def _get_module_namefrom(impfrom: ImportFrom, name: str,
                         pkgfiles: Set[str]) -> Tuple[str, bool]:
    impname = impfrom.real_name(name)
    modname = impfrom.modname
    module = impfrom.do_import_module(impfrom.modname)

    if module.path[0] not in pkgfiles:
        return (modname + '.' + impname,
                not is_standard_module(module.name))

    node = module.getattr(impname)[-1]
    attrname, is_external = _follow_name_orig(node, impname, pkgfiles)
    if not attrname:
        return (None, False)

    return (modname + '.' + attrname, is_external)


def _follow_name_orig(lookedup_node: NodeNG, name: str,
                      pkgfiles: Set[str]) -> Tuple[str, bool]:
    if isinstance(lookedup_node, Import):
        return _get_module_name(lookedup_node, name, pkgfiles)
    elif isinstance(lookedup_node, ImportFrom):
        return _get_module_namefrom(lookedup_node, name, pkgfiles)

    return (None, False)


def _inspect_name(namenode: Name, used_symbols: Set[str], pkgfiles: Set[str]):
    name = namenode.name
    _, nodelist = namenode.lookup(name)
    for node in nodelist:
        symbol_name, is_external = _follow_name_orig(node, name, pkgfiles)
        if symbol_name and is_external:
            used_symbols.add(symbol_name)


def _get_used_attr_name(instance: Instance, attrname: str) -> str:
    pkgname = instance.qname().split('.', maxsplit=1)[0]
    return '{}.class-{}.{}'.format(pkgname, classdef.name, attrname)


def _inspect_attr(attr: Attribute, used_symbols: Set[str], pkgfiles: Set[str]):
    print(f'*******attr = {attr}')
    for node in _infer_node_def(attr.expr):
        print(f'node={node}')
        if isinstance(node, Instance):
            instance = node
            if _is_external_pkg(instance.root(), pkgfiles):
                print('add' + instance)
                used_symbols.add(_get_used_attr_name(instance, attr.attrname))


def _inspect_call(call: Call, used_symbols: Set[str], pkgfiles: Set[str]):
    try:
        # Add loaded package entry if function called is
        # pkg_resources.load_entry_point. However, inference is performed only
        # the function name match to avoid costly inferrence on all function
        # calls
        func = call.func
        if (isinstance(func, Attribute) and func.attrname == 'load_entry_point'
                or isinstance(func, Name) and func.name == 'load_entry_point'):
            funcdef = next(_infer_node_def(call.func))
            if not _is_module_packaged(funcdef.root(), pkgfiles) \
                    and funcdef.qname() == 'pkg_resources.load_entry_point':
                used_symbols.add(_inspect_load_entry_point(call))

    # As python is a dynamic language, uninferable name lookup or uninferable
    # object can be common (when it highly depends on the context that we
    # cannot have without executing the code) Hence, it is safer to ignore.
    except (NameInferenceError, InferenceError, AttributeInferenceError):
        pass

    for child in call.get_children():
        _inspect_node(child, used_symbols, pkgfiles)


def _inspect_node(node, used_symbols: Set[str], pkgfiles: Set[str]):
    if isinstance(node, Call):
        _inspect_call(node, used_symbols, pkgfiles)
    elif isinstance(node, Name):
        _inspect_name(node, used_symbols, pkgfiles)
    elif isinstance(node, Attribute):
        _inspect_attr(node, used_symbols, pkgfiles)
    elif isinstance(node, Import):
        node.do_import_module(node.real_name())
    elif isinstance(node, ImportFrom):
        node.do_import_module(node.modname)
    else:
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
