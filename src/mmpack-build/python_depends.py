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

from astroid import MANAGER as astroid_manager, Uninferable
from astroid.bases import Instance
from astroid.exceptions import InferenceError, NameInferenceError, \
    AttributeInferenceError, AstroidImportError
from astroid.modutils import is_standard_module
from astroid.node_classes import Call, Attribute, Name, Expr, AssignName, \
    Import, ImportFrom, NodeNG
from mmpack_build.file_utils import is_python_script


def _is_module_packaged(mod, pkgfiles: Set[str]) -> bool:
    """
    test whether a specified module is provided by a file of the same
    mmpack package.
    """
    try:
        return mod.path and mod.path[0] in pkgfiles
    except AttributeError:
        return False


def _is_external_pkg(mod, pkgfiles: Set[str]) -> bool:
    if mod == Uninferable or path == 'builtins':
        return False

    return not (is_standard_module(mod.name)
                or _is_module_packaged(mod, pkgfiles))


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
            if base == Uninferable:
                continue

            for nodedef in base.igetattr(attr.attrname):
                yield nodedef
    else:
        for nodedef in node.infer():
            yield nodedef


def _get_module_name(imp: Import, name: str) -> Tuple[str, NodeNG]:
    name = imp.real_name(name)
    module = imp.do_import_module(name)

    return (name, module)


def _get_module_namefrom(impfrom: ImportFrom, name: str,
                         pkgfiles: Set[str]) -> Tuple[str, NodeNG]:
    impname = impfrom.real_name(name)
    modname = impfrom.modname
    module = impfrom.do_import_module(impfrom.modname)

    node = next(module.igetattr(impname))
    if not module.path or module.path[0] not in pkgfiles:
        return (modname + '.' + impname, node)

    attrname, name = _follow_name_orig(node, impname, pkgfiles)
    return (modname + '.' + attrname, node)


def _follow_name_orig(lookedup_node: NodeNG, name: str,
                      pkgfiles: Set[str]) -> Tuple[str, NodeNG]:
    if isinstance(lookedup_node, Import):
        return _get_module_name(lookedup_node, name)
    elif isinstance(lookedup_node, ImportFrom):
        return _get_module_namefrom(lookedup_node, name, pkgfiles)

    return (name, lookedup_node)


def _get_used_attr_name(instance: Instance, attrname: str) -> str:
    pkgname = instance.qname().split('.', maxsplit=1)[0]
    return '{}.class-{}.{}'.format(pkgname, instance.name, attrname)


def _follow_attr(attr: Attribute, used_symbols: Set[str],
                 pkgfiles: Set[str]) -> Tuple[str, NodeNG]:
    # Infer base and symbol name. We understand only name, attribute and
    # expression. But this should be the only nodes we encounter here.
    base = attr.expr
    sym = None
    base_node = Uninferable
    if isinstance(base, Name):
        _, nodelist = attr.frame().lookup(base.name)
        if nodelist:
            lookedup_node = nodelist[-1]
        else:
            lookedup_node = next(base.infer())
        sym, base_node = _follow_name_orig(lookedup_node, base.name, pkgfiles)
    elif isinstance(base, Attribute):
        sym, base_node = _follow_attr(base, used_symbols, pkgfiles)
    elif isinstance(base, Expr):
        base_node = next(base.infer())

    # If the base is a declared variable, just infer its value type
    if isinstance(base_node, AssignName):
        base_node = next(base_node.infer())

    # Find the public name of the attribute
    if base_node == Uninferable:
        raise InferenceError
    elif isinstance(base_node, Instance):
        sym = _get_used_attr_name(base_node, attr.attrname)
    elif sym:
        sym += '.' + attr.attrname
    else:
        raise AttributeInferenceError

    # Add symbol if base does not belong to packaged files
    if _is_external_pkg(base_node.root(), pkgfiles):
        print(f'====================== {sym}  {base_node.root().path}')
        used_symbols.add(sym)

    # Infer the type of the attribute
    try:
        node = next(base_node.igetattr(attr.attrname))
    except AttributeError:
        raise AttributeInferenceError

    return (sym, node)


def _inspect_name(namenode: Name, used_symbols: Set[str], pkgfiles: Set[str]):
    name = namenode.name
    _, nodelist = namenode.lookup(name)
    for node in nodelist:
#        if isinstance(node, Const):
#            continue

        symbol_name, node_def = _follow_name_orig(node, name, pkgfiles)
        if _is_external_pkg(node_def.root(), pkgfiles):
            used_symbols.add(symbol_name)


def _inspect_attr(attr: Attribute, used_symbols: Set[str], pkgfiles: Set[str]):
    _follow_attr(attr, used_symbols, pkgfiles)

def _inspect_call(call: Call, used_symbols: Set[str], pkgfiles: Set[str]):
    # Add loaded package entry if function called is
    # pkg_resources.load_entry_point. However, inference is performed only
    # the function name match to avoid costly inferrence on all function
    # calls
    func = call.func
    if (isinstance(func, Attribute) and func.attrname == 'load_entry_point'
            or isinstance(func, Name) and func.name == 'load_entry_point'):
        funcdef = next(_infer_node_def(call.func))
        if _is_external_pkg(funcdef.root(), pkgfiles) \
                and funcdef.qname() == 'pkg_resources.load_entry_point':
            used_symbols.add(_inspect_load_entry_point(call))

    for child in call.get_children():
        _inspect_node(child, used_symbols, pkgfiles)


def _inspect_node(node, used_symbols: Set[str], pkgfiles: Set[str]):
    try:
        if isinstance(node, Call):
            _inspect_call(node, used_symbols, pkgfiles)
        elif isinstance(node, Name):
            _inspect_name(node, used_symbols, pkgfiles)
        elif isinstance(node, Attribute):
            _inspect_attr(node, used_symbols, pkgfiles)
        else:
            for child in node.get_children():
                _inspect_node(child, used_symbols, pkgfiles)

    # As python is a dynamic language, uninferable name lookup or uninferable
    # object can be common (when it highly depends on the context that we
    # cannot have without executing the code) Hence, it is safer to ignore.
    except (NameInferenceError, InferenceError, AttributeInferenceError,
            AstroidImportError):
        pass


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
