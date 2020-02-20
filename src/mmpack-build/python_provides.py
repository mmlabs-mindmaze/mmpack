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
from typing import Set

from astroid import parse, AstroidImportError
from astroid.nodes import Import, ImportFrom, AssignName, FunctionDef, \
    ClassDef, Module


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


class PkgData:
    def __init__(self, pkgfiles: Set[str]):
        self.pkgfiles = pkgfiles
        self.syms = {}

    def is_module_packaged(self, mod: Module):
        """
        test whether a specified module is provided by the file of the same
        mmpack package.
        """
        return mod.path and mod.path[0] in self.pkgfiles

    def add_module_symbols(self, mod: Module):
        """
        Add module public symbols to the symbols exported by the package
        """
        if not self.is_module_packaged(mod) or not _is_public_sym(mod.name):
            return

        qname = mod.qname()

        # Skip adding if namespace is known
        if qname in self.syms:
            return

        self.syms[qname] = set(mod.public_names())

        # Loop over class definition and add those with a public name
        for cldef in mod.nodes_of_class(ClassDef):
            if _is_public_sym(cldef.name):
                self.add_class_symbol(cldef)

        # Loop over import statement definition and process imported modules
        for imp in mod.nodes_of_class(Import):
            for modname, _ in imp.names:
                try:
                    imp_mod = imp.do_import_module(modname)
                    self.add_module_symbols(imp_mod)
                except AstroidImportError:
                    pass

        # Loop over 'from ... import ...' definition and process imported modules
        for impfrom in mod.nodes_of_class(ImportFrom):
            try:
                imp_mod = impfrom.do_import_module(impfrom.modname)
                self.add_module_symbols(imp_mod)
            except AstroidImportError:
                pass

    def add_class_symbol(self, cldef: ClassDef):
        """
        Add class attributes symbols and ancestors if they belong to package
        """
        qname = cldef.qname()

        # Skip if class has been already processed
        if qname in self.syms:
            return

        # Add ancestors if they belong to package
        for ancestor in cldef.ancestors():
            if self.is_module_packaged(ancestor.root()):
                self.add_class_symbol(ancestor)

        syms = set()

        # Add public class attributes
        for attr in cldef.locals:
            if (isinstance(cldef.locals[attr][-1], AssignName)
                    and _is_public_sym(attr)):
                syms.add(attr)

        # Add public class methods and instance attributes
        syms.update({m.name
                     for m in cldef.mymethods() if _is_public_sym(m.name)})
        syms.update({attr
                     for attr in cldef.instance_attrs if _is_public_sym(attr)})

        self.syms[qname] = syms

def _gen_pypkg_symbols(pypkg: str, pkgdata: PkgData):
    # Parse a simple import in astroid and get the imported's module node
    imp = parse('import {}'.format(pypkg))
    mod = imp.body[0].do_import_module(pypkg)
    pkgdata.add_module_symbols(mod)

    # For a python package, __main__.py holds the contents which will be
    # executed when the module is run with -m. Hence we test the capability of
    # a package to be runnable by trying to import the <pypkg>.__main__ module.
    # If the import is possible <pypkg>.__main__ will be added to the public
    # symbols.
    try:
        main_modname = pypkg + '.__main__'
        imp = parse('import ' + main_modname)
        mod = imp.body[0].do_import_module(main_modname)
        pkgdata.add_module_symbols(mod)
    except AstroidImportError:
        pass


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
    parser.add_argument('infiles', type=str, nargs='*')

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
    pkgdata = PkgData({abspath(f.strip()) for f in options.infiles})

    _gen_pypkg_symbols(options.pypkgname, pkgdata)

    # Return result on stdout
    for namespace in sorted(pkgdata.syms):
        for sym in sorted(pkgdata.syms[namespace]):
            print(namespace + '.' + sym)


if __name__ == '__main__':
    main()
