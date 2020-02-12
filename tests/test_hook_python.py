# @mindmaze_header@

import unittest
from glob import glob
from os.path import dirname, abspath, join
from typing import Set

from mmpack_build.base_hook import PackageInfo
from mmpack_build.hook_python import _gen_pysymbols, _gen_pydepends


_testdir = dirname(abspath(__file__))
_sitedir = join(_testdir, 'pydata')


def _load_py_symbols(name: str, pkgfiles: Set[str]) -> Set[str]:
    pkg = PackageInfo('test_pkg')
    pkg.files = {join(_sitedir, f) for f in pkgfiles}

    return _gen_pysymbols(name, pkg, _sitedir)


def _get_py_imports(pkgfiles: Set[str]) -> Set[str]:
    pkg = PackageInfo('test_pkg')
    pkg.files = {join(_sitedir, f) for f in pkgfiles}
    used_symbols = _gen_pydepends(pkg, _sitedir)
    return {s.split('.', maxsplit=1)[0] for s in used_symbols}


class TestPythonHook(unittest.TestCase):

    def test_provides_bare_module(self):
        """test provides module without package folder"""
        pkgfiles = ['bare.py']
        refsymbols = {
            'bare.A_CLASS': '',
            'bare.EXPORTED_LIST': '',
            'bare.MainData': '',
            'bare.MainData.__init__': '',
            'bare.MainData.a_class_attr': '',
            'bare.MainData.data1': '',
            'bare.MainData.disclose_private': '',
            'bare.MainData.fullname': '',
            'bare.THE_ANSWER': '',
            'bare.main_dummy_fn': '',
        }
        syms = _load_py_symbols('bare', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_simple(self):
        """test provides no import"""
        pkgfiles = ['simple/__init__.py']
        refsymbols = {
            'simple.A_CLASS': '',
            'simple.EXPORTED_LIST': '',
            'simple.MainData': '',
            'simple.MainData.__init__': '',
            'simple.MainData.a_class_attr': '',
            'simple.MainData.data1': '',
            'simple.MainData.disclose_private': '',
            'simple.MainData.fullname': '',
            'simple.THE_ANSWER': '',
            'simple.main_dummy_fn': '',
        }
        syms = _load_py_symbols('simple', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_multi(self):
        """test provides pkg with multiple modules"""
        pkgfiles = [
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refsymbols = {
            'multi.FooBar': '',
            'multi.FooBar.__init__': '',
            'multi.FooBar.fullname': '',
            'multi.FooBar.hello': '',
            'multi.FooBar.new_data': '',
            'multi.MainData': 'multi.foo.MainData',
            'multi.MainData.__init__': 'multi.foo.MainData.__init__',
            'multi.MainData.a_class_attr': 'multi.foo.MainData.a_class_attr',
            'multi.MainData.data1': 'multi.foo.MainData.data1',
            'multi.MainData.disclose_private': 'multi.foo.MainData.disclose_private',
            'multi.MainData.fullname': 'multi.foo.MainData.fullname',
            'multi.argh': '',
            'multi.bar.A_BAR': '',
            'multi.bar.Bar': 'multi..bar.Bar',
            'multi.bar.Bar.__init__': 'multi..bar.Bar.__init__',
            'multi.bar.Bar.drink': 'multi..bar.Bar.drink',
            'multi.bar.print_hello': 'multi..bar.print_hello',
            'multi.main_dummy_fn': 'multi.foo.main_dummy_fn',
        }
        syms = _load_py_symbols('multi', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_pkg_imported(self):
        """test provides pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refsymbols = {
            'pkg_imported.FooBar': '',
            'pkg_imported.FooBar.__init__': '',
            'pkg_imported.FooBar.fullname': '',
            'pkg_imported.FooBar.hello': '',
            'pkg_imported.FooBar.new_data': '',
            'pkg_imported.argh': '',
        }
        syms = _load_py_symbols('pkg_imported', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_depends_import_simple(self):
        """test dependent imports with simple package with no import"""
        pkgfiles = ['simple/__init__.py']
        refimports = set()
        imports = _get_py_imports(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_import_multi(self):
        """test dependent import with multiple modules"""
        pkgfiles = [
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refimports = set()
        imports = _get_py_imports(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_import_pkg_imported(self):
        """test dependent import with pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refimports = {'simple'}
        imports = _get_py_imports(pkgfiles)
        self.assertEqual(imports, refimports)
