#!/usr/bin/env python3
# @mindmaze_header@
"""
python depends extractor.

Given a set of files that belong to the same mmpack package, this utility will
scan the python modules and register the symbol used from other modules out of
the mmpack package.

It will print on standard output the qualified name of the public symbols used.
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath
from typing import Set, Iterator, Tuple, Union

from astroid import MANAGER as astroid_manager
from astroid import Uninferable, Module, Instance, ClassDef, \
    Import, ImportFrom, Call, Attribute, Name
from astroid.exceptions import InferenceError, NameInferenceError, \
    AttributeInferenceError, AstroidImportError
from astroid.modutils import is_standard_module
from astroid.node_classes import NodeNG
from astroid.objects import Super


def _is_builtin(node: NodeNG, builtin_typename=None):
    try:
        pytype = node.pytype()
        if builtin_typename:
            return pytype == 'builtins.' + builtin_typename

        return pytype.startswith('builtins.')
    except AttributeError:
        return False


def _get_classbase_defining_attr(cldef: Union[Instance, ClassDef],
                                 attrname: str) -> ClassDef:
    node = Uninferable

    # Get the first node that define an instance attribute (even in ancestors)
    try:
        node = cldef.instance_attr(attrname)[0]
    except AttributeInferenceError:
        pass

    # If previous attribute has not been found, get the first node that define
    # an class attribute (even in ancestors)
    try:
        if node == Uninferable:
            node = cldef.local_attr(attrname)[0]
    # old-style classes may raise NotImplementedError.
    except NotImplementedError:
        raise AttributeInferenceError

    # Get the class node defining the node found
    base = node.frame()
    while not isinstance(base, ClassDef):
        if not base.parent:
            raise AttributeInferenceError
        base = base.parent

    return base


def _resolve_super_with_attr(super_node: Super,
                             attrname: str) -> ClassDef:
    """
    lookup in the proxy for super builtin for the specified attribute name

    Args:
        super_node: proxy for super()
        attrname: name of attribute to look

    Returns:
        the base_class that defines the attribute
    """
    for classnode in super_node.super_mro():
        try:
            return _get_classbase_defining_attr(classnode, attrname)
        except AttributeInferenceError:
            pass

    # No attribute has been found
    raise AttributeInferenceError


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


class DependsInspector:
    # pylint: disable=too-few-public-methods
    """
    Class allowing to collect the external dependencies of python modules
    """
    def __init__(self, pkgfiles: Set[str]):
        self.pkgfiles = pkgfiles
        self.used_symbols = set()
        self.failed_imports = set()

    def _is_local_module(self, mod: Module) -> bool:
        """
        test whether a specified module is provided by a file of the same
        mmpack package.
        """
        try:
            return mod.path and mod.path[0] in self.pkgfiles
        except AttributeError:
            return False

    def _is_external_pkg(self, node: NodeNG) -> bool:
        """
        Test whether a node is imported from an external package (ie neither
        standard library or local module)
        """
        mod = node.root()
        if not isinstance(mod, Module):
            return False

        return not (is_standard_module(mod.name) or self._is_local_module(mod))

    def _get_module_namefrom(self, impfrom: ImportFrom,
                             name: str) -> Tuple[str, NodeNG]:
        """
        Resolve name defined in a "from ... import ..." statement.
        If a name is defined in an import, do the import and return recursively
        the node in the imported module. The name resolution will be stopped at
        the first external package.

        Args:
            imp: the Import statement node that define name
            name: Name defined in the 'import ... from' statement.

        Returns: the couple of public symbol name and node definition
        """
        real_name = impfrom.real_name(name)
        module = impfrom.do_import_module(impfrom.modname)
        node = next(module.igetattr(real_name))

        # If the pointed node does not belong to package, just report the
        # public name as it is known now
        if not self._is_local_module(module):
            return (impfrom.modname + '.' + real_name, node)

        # Module defining the name is in the package so check the name is not
        # generated from an import done in this module
        attrname, name = self._follow_name_origin(node, real_name)
        return (impfrom.modname + '.' + attrname, node)

    def _follow_name_origin(self, node: NodeNG,
                            name: str) -> Tuple[str, NodeNG]:
        """
        Resolve node definition.
        If a name is defined in an import, follow do the import and return the
        node in the imported module. Otherwise, just report the name and node
        as is. The name resolution will be stopped at the first external
        package.

        Args:
            node: the lookup up statement node that define name
            name: Name defined in looked up statement node

        Returns: the couple of public symbol name and node definition
        """
        if isinstance(node, Import):
            imp = node
            name = imp.real_name(name)
            node = imp.do_import_module(name)
        elif isinstance(node, ImportFrom):
            return self._get_module_namefrom(node, name)

        return (name, node)

    def _inspect_node_attribute(self, attr: Attribute):
        """
        Add the public name of attribute to used symbols set if defined in
        external package

        Args:
            attr: the Attribute node to inspect
        """
        for base in _infer_node_def(attr.expr):
            # Find the node that actually define the attribute
            if isinstance(base, Super):
                base = _resolve_super_with_attr(base, attr.attrname)
            elif isinstance(base, (Instance, ClassDef)):
                base = _get_classbase_defining_attr(base, attr.attrname)

            # Add symbol if base does not belong to packaged files
            if self._is_external_pkg(base) and not _is_builtin(base, 'tuple'):
                sym = base.qname() + '.' + attr.attrname
                self.used_symbols.add(sym)

    def _inspect_node_name(self, namenode: Name):
        """
        Add the public name to used symbols set if defined in
        external package

        Args:
            namenode: the Name node to inspect
        """
        name = namenode.name
        _, nodelist = namenode.lookup(name)
        for node in nodelist:
            symbol_name, node_def = self._follow_name_origin(node, name)
            if self._is_external_pkg(node_def):
                self.used_symbols.add(symbol_name)

    def _inspect_node_call(self, call: Call):
        # Add loaded package entry if function called is
        # pkg_resources.load_entry_point. However, inference is performed only
        # the function name match to avoid costly inference on all function
        # calls
        func = call.func
        if not (isinstance(func, Attribute)
                and func.attrname == 'load_entry_point'
                or isinstance(func, Name)
                and func.name == 'load_entry_point'):
            return

        funcdef = next(_infer_node_def(call.func))
        if self._is_external_pkg(funcdef) \
                and funcdef.qname() == 'pkg_resources.load_entry_point':
            # Add main entry of imported package
            pkgreq = next(call.args[0].infer()).value
            pkgname = pkgreq.split('==')[0]
            self.used_symbols.add(pkgname + '.__main__')

    def _inspect_node(self, node: NodeNG):
        """
        Add the public name to used symbols set if node statement use external
        symbol

        Args:
            node: the node to inspect
        """
        try:
            if isinstance(node, Call):
                self._inspect_node_call(node)
            elif isinstance(node, Name):
                self._inspect_node_name(node)
            elif isinstance(node, Attribute):
                self._inspect_node_attribute(node)

        # As python is a dynamic language, uninferable name lookup or
        # uninferable object can be common (when it highly depends on the
        # context that we cannot have without executing the code) Hence, it is
        # safer to ignore.
        except (NameInferenceError, InferenceError, AttributeInferenceError):
            pass
        except AstroidImportError as import_error:
            # If modname is attribute, this is an actual importation error (not
            # a too many level error). Hence in this case, the module name is
            # collected for reporting at the end of program
            modname = getattr(import_error, 'modname', None)
            if modname:
                self.failed_imports.add(modname)

    def gather_pyfile_depends(self, filename: str):
        """
        Load python module and inspect its used symbols
        """
        try:
            tree = astroid_manager.ast_from_file(filename)
        except SyntaxError:
            print('{} failed to be parsed'.format(filename), file=sys.stderr)
            return

        for node in tree.nodes_of_class((Call, Name, Attribute)):
            self._inspect_node(node)


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--site-path', dest='site_path', type=str, nargs='?',
                        help='path of python site-packages or folder '
                        'containing python package')
    parser.add_argument('infiles', type=str, nargs='*')

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

    pkgfiles = [abspath(f.strip()) for f in options.infiles]

    inspector = DependsInspector(pkgfiles)
    for filename in pkgfiles:
        inspector.gather_pyfile_depends(filename)

    # Return sorted results on stdout
    for sym in sorted(inspector.used_symbols):
        print(sym)

    if inspector.failed_imports:
        print('Warning: Following modules failed to be imported:\n    {}'
              .format('\n    '.join(sorted(inspector.failed_imports))),
              file=sys.stderr)


if __name__ == '__main__':
    main()
