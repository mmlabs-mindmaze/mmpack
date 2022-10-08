# @mindmaze_header@
"""
python depends extractor.

Given a set of files that belong to the same mmpack package, this utility will
scan the python modules and register the symbol used from other modules out of
the mmpack package.

It will print on standard output a JSON mapping of, for each external python
package used, the list of qualified name of the public symbols used.
"""

import json
import sys
from os.path import abspath, dirname, exists, join as join_path
from functools import cache
from traceback import print_exc
from typing import List, Iterable, Iterator, Optional, Set, Tuple, Union

import pkg_resources

from astroid import MANAGER as astroid_manager
from astroid import Uninferable, Module, Instance, ClassDef, \
    Import, ImportFrom, Call, Attribute, Name
from astroid.exceptions import AttributeInferenceError, AstroidImportError, \
    InconsistentMroError, InferenceError, NameInferenceError
from astroid.modutils import (is_standard_module, is_namespace,
                              modpath_from_file,
                              modpath_from_file_with_callback,
                              file_info_from_modpath)
from astroid.node_classes import NodeNG
from astroid.objects import Super


@cache
def _is_namespace_pkg(modname: str) -> bool:
    """reports true if modname is a PEP420 namespace package"""
    try:
        is_ns = is_namespace(file_info_from_modpath(modname.split('.')))
    except ImportError:
        is_ns = False
    return is_ns


def _is_standard_module(mod: Module) -> bool:
    # verify root package is not a PEP420 namespace package
    # (is_standard_module is fooled by them)
    if _is_namespace_pkg(mod.name.split('.')[0]):
        return False

    return is_standard_module(mod.name)


def _is_pkg(path: str, mod_path: List[str]) -> bool:
    modpath = []
    for part in mod_path:
        modpath.append(part)
        path = join_path(path, part)
        if not exists(join_path(path, '__init__.py')):
            if not _is_namespace_pkg('.'.join(modpath)):
                return False
    return True


def _load_pyfile_module(filename: str) -> Optional[Module]:
    try:
        modpath = modpath_from_file_with_callback(filename, None, _is_pkg)
    except ImportError:
        modpath = [filename]

    try:
        return astroid_manager.ast_from_file(filename, '.'.join(modpath))
    except (SyntaxError, InconsistentMroError) as error:
        print(f'{filename} failed to be parsed: {error}', file=sys.stderr)
    except AstroidImportError:
        pass

    return None


def _belong_to_public_package(filename: str):
    """
    Test a file is a submodule of a public package, ie submodule accessible
    through sys.path.
    """
    try:
        modpath_from_file(filename)
        return True
    except ImportError:
        pass

    return False


def _is_builtin(node: NodeNG, builtin_typename=None):
    try:
        pytype = node.pytype()
        if builtin_typename:
            return pytype == 'builtins.' + builtin_typename

        return pytype.startswith('builtins.')
    except AttributeError:
        return False


def _get_classbase_defining_attr(cldef: Union[Instance, ClassDef, Super],
                                 attrname: str) -> ClassDef:

    # Get the base of the first node defining the attribute that has an
    # inspectable parent (skip NoneType, builtins...). Search is done even in
    # ancestors).
    for node in cldef.getattr(attrname):
        try:
            # Search the first parent that defines a class (this allows to
            # handle properly instance attribute defined in __init__ for
            # example)
            base = node.frame()
            while not isinstance(base, ClassDef):
                base = base.parent

            return base

        # If node is an implicit attribute, calling .frame() or .parent will
        # eventually fails due to missing method. Then we just need to consider
        # the next candidate
        except AttributeError:
            continue

    return Uninferable


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
    def __init__(self, pkgfiles: Iterable[str]):
        self.pkgfiles = {abspath(f) for f in pkgfiles}
        self.used_symbols = set()
        self.failed_imports = set()

    def _is_local_module(self, mod: Module) -> bool:
        """
        test whether a specified module is provided by a file of the same
        mmpack package.
        """
        try:
            if not mod.path:
                return False
            if isinstance(mod.path, list):
                return mod.path[0] in self.pkgfiles
            elif isinstance(mod.path, str):
                return mod.path in self.pkgfiles
            else:
                return False
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

        return not (_is_standard_module(mod) or self._is_local_module(mod))

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

        try:
            nodelist = module.getattr(real_name)
        except AttributeInferenceError:
            _, nodelist = module.lookup(real_name)

        for node in nodelist:
            if not node or node is impfrom:
                continue

            # If the pointed node does not belong to package, just report the
            # public name as it is known now
            if not self._is_local_module(module):
                return (impfrom.modname + '.' + real_name, node)

            attrname, name = self._follow_name_origin(node, real_name)
            return (impfrom.modname + '.' + attrname, node)

        # If getattr or lookup fails, just report the resolved module as
        # defining node. This is sufficient to identify whether a symbol is
        # local or external
        return (impfrom.modname + '.' + real_name, module)

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
        try:
            if isinstance(node, Import):
                imp = node
                name = imp.real_name(name)
                node = imp.do_import_module(name)
            elif isinstance(node, ImportFrom):
                return self._get_module_namefrom(node, name)
        except RecursionError:
            pass

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
            if isinstance(base, (Instance, ClassDef, Super)):
                base = _get_classbase_defining_attr(base, attr.attrname)

            # Add symbol if base does not belong to packaged files
            if self._is_external_pkg(base) and not _is_builtin(base, 'tuple'):
                qname = base.qname()
                # skip base whose origin could not be found (mostly likely to
                # astroid transform that has dropped the module of the original
                # node)
                if not qname.startswith('.'):
                    self.used_symbols.add(qname + '.' + attr.attrname)

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
        """
        Analyze call statement and search for call to load_entry_point()
        function from pkg_resources. For each one found, the imported module
        __main__ entry is added to the used symbols
        """
        # Do a first rough scan to check if function is likely to be
        # load_entry_point. This is done to avoid  costly inference on all
        # function calls
        func = call.func
        if not (isinstance(func, Attribute)
                and func.attrname == 'load_entry_point'
                or isinstance(func, Name)
                and func.name == 'load_entry_point'):
            return

        # Do actual inference of the function call and verify the called
        # function is external and indeed pkg_resources.load_entry_point. If
        # so, add imported module __main__ entry.
        funcdef = next(_infer_node_def(func))
        if self._is_external_pkg(funcdef) \
                and funcdef.qname() == 'pkg_resources.load_entry_point':
            # Add main entry of imported package
            args = [next(a.infer()).value for a in call.args]
            entry = pkg_resources.get_entry_info(*args)
            sym = entry.module_name
            if entry.attrs:
                sym += '.' + '.'.join(entry.attrs)
            self.used_symbols.add(sym)

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
            else:
                return

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

    def _gather_pyfile_depends(self, filename: str):
        """
        Load python module and inspect its used symbols
        """
        tree = _load_pyfile_module(filename)
        if tree is None:
            return

        # If not public module, add directory to simulate calling the script
        # directly
        is_public_submodule = _belong_to_public_package(filename)
        if not is_public_submodule:
            sys.path.insert(0, dirname(filename))

        for node in tree.nodes_of_class((Call, Name, Attribute)):
            self._inspect_node(node)

        # Reverting sys.path if modified
        if not is_public_submodule:
            sys.path.pop(0)

    def gen_depends(self):
        """
        Inspect loaded python files and generate their dependency list
        """
        pyfiles = sorted(self.pkgfiles)

        for filename in pyfiles:
            _load_pyfile_module(filename)

        for filename in pyfiles:
            try:
                self._gather_pyfile_depends(filename)
            except Exception:  # pylint: disable=broad-except
                print(f'Warning: Analysis of {filename} raises an exception:',
                      file=sys.stderr)
                print_exc(file=sys.stderr)
                print(' This is possibly an bug from astroid\n'
                      f' => Skipping {filename} processing for depends',
                      file=sys.stderr)

    def used_pkgs(self) -> Iterator[Tuple[str, Set[str]]]:
        """
        Get an iterator of the tuple holding the external module name used
        along with their set of symbols.
        """
        reported_pkgs = set()
        for modname in self.used_symbols:
            # Find the first parent package that is not a namespace
            modpath = modname.split('.')
            pypkg = modpath.pop(0)
            while _is_namespace_pkg(pypkg):
                pypkg += '.' + modpath.pop(0)

            if pypkg in reported_pkgs:
                continue

            syms = {s for s in self.used_symbols if s.startswith(pypkg+'.')}
            yield (pypkg, syms)
            reported_pkgs.add(pypkg)


def add_to_pkg_resources(entry):
    """
    Add path entry to the list of python package provider known by
    pkg_resources. If a package were already known (as parsed by initial
    sys.path), it will be replaced.
    """
    working_set = pkg_resources.working_set
    working_set.entry_keys.setdefault(entry, [])
    working_set.entries.append(entry)
    for dist in pkg_resources.find_distributions(entry, True):
        working_set.add(dist, entry, replace=True)


def run_depends(input_files: Iterable[str], sitedirs: Iterable[str]):
    """
    python_depends utility entry point
    """
    # If some site path folders are specified, make them known by pkg_resources.
    for sitedir in sitedirs:
        add_to_pkg_resources(abspath(sitedir))

    inspector = DependsInspector(input_files)
    inspector.gen_depends()

    # Return results as JSON dict on stdout
    json.dump({k: list(sorted(v)) for k, v in inspector.used_pkgs()},
              fp=sys.stdout, indent='    ', separators=(',', ': '))

    if inspector.failed_imports:
        print('Warning: Following modules failed to be imported. They may be '
              'optional imports:\n    '
              + '\n    '.join(sorted(inspector.failed_imports)),
              file=sys.stderr)
