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
from os.path import abspath, basename, dirname
from typing import Set, Union

from astroid import AstroidImportError, AstroidSyntaxError, MANAGER
from astroid import InconsistentMroError
from astroid.nodes import Import, ImportFrom, AssignName, ClassDef, Module


def _is_public_sym(name: str) -> bool:
    """
    Test from name if symbol is public or implementation details.
    With this test, we consider foo, foo_bar as public symbol, _foo, _foo_bar
    as private symbol and __foo and __foo_bar as public. The rationale of
    matching '__' prefix as public is for handling special methods like
    __init__, _setitem__, __getitem__....  Now we ignore the case of '___'
    prefix (3 underscores) and consider them as public.
    """
    for comp in name.split('.'):
        if comp.startswith('_') and not comp.startswith('__'):
            return False

    return True


class PkgData:
    """
    Class holding the explored symbols so far and the boundaries of the mmpack
    package being analyzed (ie, which python module is actually copackaged).
    """
    def __init__(self, pkgfiles: Set[str]):
        self.pkgfiles = pkgfiles
        self.syms = {}

    def is_module_packaged(self, mod: Module):
        """
        test whether a specified module is provided by the file of the same
        mmpack package.
        """
        return mod.path and mod.path[0] in self.pkgfiles

    def _add_import(self, imp: Union[Import, ImportFrom], names=None):
        if names is None:
            names = [n for n, _ in imp.names]

        for modname in names:
            try:
                imp_mod = imp.do_import_module(modname)
                self.add_module_public_symbols(imp_mod)
            except AstroidImportError:
                # If module cannot be imported, this should an external module,
                # Let's ignore as no provided symbols will be found there
                pass

    def _add_importfrom(self, impfrom: ImportFrom):
        try:
            mod = impfrom.do_import_module(impfrom.modname)
            self.add_module_public_symbols(mod)
        except AstroidImportError:
            # If module cannot be imported, this should an external module,
            # hence we don't have to search for provided symbols there.
            return

        # Get the actual list of imported names (expand wildcard import)
        imported_names = [n for n, _ in impfrom.names]
        if imported_names[0] == '*':
            imported_names = mod.wildcard_import_names()

        # If 'from' name is absolute, imported module name must be prefixed
        # (import is absolute if level is 0 or None)
        mod_prefix = ''
        if not impfrom.level:
            mod_prefix = impfrom.modname + '.'

        # Import module for all names that are not defined in 'from' module
        names = [mod_prefix + n for n in imported_names if n not in mod.keys()]
        self._add_import(impfrom, names)

    def _add_class_public_symbols(self, cldef: ClassDef):
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
                self._add_class_public_symbols(ancestor)

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

    def add_module_public_symbols(self, mod: Module):
        """
        Add module public symbols to the symbols exported by the package
        """
        if not self.is_module_packaged(mod) or not _is_public_sym(mod.name):
            return

        qname = mod.qname()

        # Skip namespace has already been processed
        if qname in self.syms:
            return

        # Add all public symbols of the namespace, only if the module has its
        # path well identified
        if qname != mod.file:
            self.syms[qname] = set(mod.public_names())

            # Loop over class definition and add those with a public name
            for cldef in mod.nodes_of_class(ClassDef):
                if _is_public_sym(cldef.name):
                    self._add_class_public_symbols(cldef)

        # Loop over import statement definition and process imported modules
        for imp in mod.nodes_of_class(Import):
            try:
                self._add_import(imp)
            except RecursionError:
                print('Recursion error, maybe astroid bug...'
                      'Skipping provides processing', file=sys.stderr)

        # Loop over 'from ... import ...' definition and process imported
        # modules
        for impfrom in mod.nodes_of_class(ImportFrom):
            try:
                self._add_importfrom(impfrom)
            except RecursionError:
                print('Recursion error, maybe astroid bug...'
                      'Skipping provides processing', file=sys.stderr)

    def add_namespace_symbol(self, namespace: str, symbol: str):
        """
        Add a symbol to a specific namespace. It does not exists yet, it will
        be created.
        """
        ns_symset = self.syms.setdefault(namespace, set())
        ns_symset.add(symbol)

    def gen_pypkg_symbols(self):
        """
        Generate set symbols of python package
        """
        pyfiles = {f for f in self.pkgfiles if f.endswith('.py')}

        # Generate process order
        processing_list = []
        for initfile in [f for f in pyfiles if basename(f) == '__init__.py']:
            # Process first init file
            processing_list.append(initfile)
            pyfiles.discard(initfile)

            # Process all files that in the package
            subdir = dirname(initfile)
            for pyfile in {f for f in pyfiles if dirname(f) == subdir}:
                processing_list.append(pyfile)
                pyfiles.discard(pyfile)

        # Process all single module file (those at the level of sitedir)
        processing_list += [f for f in pyfiles if dirname(f) in sys.path]

        for pyfile in processing_list:
            try:
                mod = MANAGER.ast_from_file(pyfile)
            except (AstroidSyntaxError, InconsistentMroError) as error:
                print(f'Warning: {pyfile} has raised a syntax error:\n'
                      f' {error}\n'
                      ' => Skipping its processing for provides',
                      file=sys.stderr)
                continue

            self.add_module_public_symbols(mod)

            # For a python package, __main__.py holds the contents which
            # will be executed when the module is run with -m. If module
            # exists, <pypkg>.__main__ will be added to the public symbols.
            if basename(pyfile) == '__main__.py':
                pkg_namespace = mod.qname().rsplit('.', 1)[0]
                self.add_namespace_symbol(pkg_namespace, '__main__')


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
    python_provides utility entry point
    """
    options = parse_options()

    # If site path folders are specified, add them to sys.path so that astroid
    # resolve the imports properly
    for sitedir in options.site_paths:
        sys.path.insert(0, abspath(sitedir))

    # Load list of files in package from stdin
    pkgdata = PkgData({abspath(f.strip()) for f in options.infiles})
    pkgdata.gen_pypkg_symbols()

    # Return result on stdout
    for namespace in sorted(pkgdata.syms):
        for sym in sorted(pkgdata.syms[namespace]):
            print(namespace + '.' + sym)


if __name__ == '__main__':
    main()
