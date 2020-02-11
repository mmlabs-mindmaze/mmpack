#!/usr/bin/env python3
# @mindmaze_header@
"""
python package provide extractor.

Given a python package and a set of files that belong to the same mmpack
package, this utility will scan the public API and output the different symbols
defined in it. The program will try to show only what is public. Anything that
is exposed in submodules and not exposed in the __init__.py file is assumed
to be internal implementation detail.

This script is meant to be called by an external process, not within the
process that run the mmpack-build scripts. The main reason is to be able to
handle project that would need a version of python superior to the one running
mmpack-buid (possibly the system's one). In such case, having this script in a
separated process allows to list the new python version as build depends of the
package and use this one to run the script.
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath
from typing import Set, Dict

from astroid import parse, AstroidImportError
from astroid.nodes import Import, ImportFrom, AssignName, FunctionDef, \
    ClassDef, Module


PySyms = Dict[str, str]


def _is_module_packaged(mod, pkgfiles: Set[str]) -> bool:
    """
    test whether a specified module is provided by the file of the same
    mmpack package.
    """
    return mod.path and mod.path[0] in pkgfiles


def _is_public_sym(name: str) -> bool:
    """
    Test from name if symbol is public or implementation details.
    With this test, we consider foo, foo_bar as public symbol, _foo, _foo_bar
    as private symbol and __foo and __foo_bar as public. The rationale of
    matching '__' prefix as public is for handling special methods like
    __init__, _setitem__, __getitem__....  Now we ignore the case of '___'
    prefix (3 underscores) and consider them as public.
    """
    return name.startswith('__') or not name.startswith('_')


def _process_class_node(cldef: ClassDef, pkgfiles: Set[str]) -> PySyms:
    """
    Determine the symbols provided by the class. The returned symbols will
    include the attributes and methods of the class prefixed by the class name
    like:
        cls_name
        cls_name.attr1
        cls_name.attr2
        cls_name.__init__
        cls_name.method1
        cls_name.method2

    Args:
        cldef: the astroid node defining the class
        pkgfiles: set of files in the same mmpack package

    Returns:
        set of name defined by the class
    """
    syms = {}
    name_prefix = cldef.name + '.'
    qname_prefix = cldef.qname() + '.'

    # Add public class attributes
    for attr in cldef.locals:
        if (isinstance(cldef.locals[attr][-1], AssignName)
                and _is_public_sym(attr)):
            syms[name_prefix + attr] = qname_prefix + attr

    # Add public class methods and instance attributes
    syms.update({name_prefix + m.name: m.qname()
                 for m in cldef.mymethods() if _is_public_sym(m.name)})
    syms.update({name_prefix + a: qname_prefix + a
                 for a in cldef.instance_attrs if _is_public_sym(a)})

    syms[cldef.name] = cldef.qname()
    return syms


def _process_import_from(impfrom: ImportFrom, name: str,
                         pkgfiles: Set[str]) -> PySyms:
    realname = impfrom.real_name(name)

    # First try to load the import as module, if not present, let's try to
    # import a symbol
    try:
        mod = impfrom.do_import_module(impfrom.modname + '.' + realname)
        if not _is_module_packaged(mod, pkgfiles):
            return set()

        syms = {}
        for pub_name in mod.public_names():
            syms.update(_get_provides_from_name(mod, pub_name, pkgfiles))
        return {name + '.' + s: qname for s, qname in syms.items()}
    except AstroidImportError:
        # If we reach here modname.realname cannot be imported as module. Hence
        # we must then try to load a symbol called realname from the package
        # named modname
        pass

    mod = impfrom.do_import_module(impfrom.modname)
    if not _is_module_packaged(mod, pkgfiles):
        return set()

    # Do lookup in the package and generate symbols
    syms = _get_provides_from_name(mod, realname, pkgfiles)
    if realname == name:
        return syms

    # replace "real name" prefix by "as name"
    as_syms = {}
    for sym, qname in syms.items():
        # get the suffix part of prefix.suffix pattern in sym. If sym does not
        # contain '.', suffix will be ''
        suffix = sym.split(sep='.', maxsplit=1)[1:]
        as_name = name + '.' + suffix if suffix else name
        as_syms[as_name] = qname
    return as_syms


def _get_provides_from_name(mod: Module, name: str, pkgfiles: Set[str]):
    _, node_list = mod.lookup(name)
    if not node_list:
        return {}
    node = node_list[-1]

    if isinstance(node, Import):
        return {}  # import are always outside of package
    elif isinstance(node, ImportFrom):
        return _process_import_from(node, name, pkgfiles)
    elif isinstance(node, ClassDef):
        return _process_class_node(node, pkgfiles)
    elif isinstance(node, FunctionDef):
        return {node.name: node.qname()}
    elif isinstance(node, AssignName):
        return {node.name: ''}

    raise AssertionError('Unsupported type of public symbol: {}'.format(name))


def _gen_pypkg_symbols(pypkg: str, pkgfiles: Set[str]) -> PySyms:

    # Parse a simple import in astroid and get the imported's module node
    imp = parse('import {}'.format(pypkg))
    mod = imp.body[0].do_import_module(pypkg)

    # Inspect the provides only of the wildcard symbols (ie the public symbols,
    # reduced to the one listed in the __all__ list if present)
    symbols = {}
    for name in mod.wildcard_import_names():
        symbols.update(_get_provides_from_name(mod, name, pkgfiles))

    return symbols


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--site-path', dest='site_path', type=str, nargs='?',
                        help='path of python site-packages or folder '
                        'containing python package')
    parser.add_argument('pypkgname', type=str,
                        help='name of the python package to inspect (name '
                        'supplied to import statement)')

    return parser.parse_args()


def main():
    """
    python_provides utility entry point
    """
    options = parse_options()

    # If site path folder is specified, add it to sys.path so astroid resolve
    # the imports properly
    if options.site_path:
        sys.path.insert(0, abspath(options.site_path))

    # Load list of files in package from stdin
    pkgfiles = {abspath(f.strip()) for f in sys.stdin.readlines()}

    symbols = _gen_pypkg_symbols(options.pypkgname, pkgfiles)

    # Return result on stdout
    for sym, qname in symbols.items():
        print('{}: {}'.format(sym, qname))


if __name__ == '__main__':
    main()
