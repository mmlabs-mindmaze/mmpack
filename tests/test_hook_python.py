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


def _get_py_depends(pkgfiles: Set[str]) -> Set[str]:
    pkg = PackageInfo('test_pkg')
    pkg.files = {join(_sitedir, f) for f in pkgfiles}
    used_symbols = _gen_pydepends(pkg, _sitedir)
    return used_symbols


class TestPythonHook(unittest.TestCase):

    def test_provides_bare_module(self):
        """test provides module without package folder"""
        pkgfiles = ['bare.py']
        refsymbols = {
            'bare.A_CLASS': '',
            'bare.EXPORTED_LIST': '',
            'bare.THE_ANSWER': '',
            'bare.main_dummy_fn': '',
            'bare.MainData': '',
            'bare.MainData.a_class_attr': '',
            'bare.MainData.__init__': '',
            'bare.MainData.data1': '',
            'bare.MainData.fullname': '',
            'bare.MainData.disclose_private': '',
        }
        syms = _load_py_symbols('bare', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_simple(self):
        """test provides no import"""
        pkgfiles = ['simple/__init__.py']
        refsymbols = {
            'simple.A_CLASS': '',
            'simple.EXPORTED_LIST': '',
            'simple.THE_ANSWER': '',
            'simple.main_dummy_fn': '',
            'simple.MainData': '',
            'simple.MainData.a_class_attr': '',
            'simple.MainData.__init__': '',
            'simple.MainData.data1': '',
            'simple.MainData.fullname': '',
            'simple.MainData.disclose_private': '',
        }
        syms = _load_py_symbols('simple', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_multi(self):
        """test provides pkg with multiple modules"""
        pkgfiles = [
            'multi/__main__.py',
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refsymbols = {
            'multi.argh': '',
            'multi.main_dummy_fn': 'multi.foo.main_dummy_fn',
            'multi.MainData': 'multi.foo.MainData',
            'multi.MainData.a_class_attr': 'multi.foo.MainData.a_class_attr',
            'multi.MainData.__init__': 'multi.foo.MainData.__init__',
            'multi.MainData.data1': 'multi.foo.MainData.data1',
            'multi.MainData.fullname': 'multi.foo.MainData.fullname',
            'multi.MainData.disclose_private': 'multi.foo.MainData.disclose_private',
            'multi.FooBar': '',
            'multi.FooBar.a_class_attr': 'multi.foo.MainData.a_class_attr',
            'multi.FooBar.__init__': '',
            'multi.FooBar.data1': 'multi.foo.MainData.data1',
            'multi.FooBar.fullname': '',
            'multi.FooBar.new_data': '',
            'multi.FooBar.disclose_private': 'multi.foo.MainData.disclose_private',
            'multi.FooBar.hello': '',
            'multi.bar.print_hello': 'multi..bar.print_hello',
            'multi.bar.Bar': 'multi..bar.Bar',
            'multi.bar.Bar.__init__': 'multi..bar.Bar.__init__',
            'multi.bar.Bar.drink': 'multi..bar.Bar.drink',
            'multi.bar.A_BAR': '',
            'multi.__main__': '',
        }
        syms = _load_py_symbols('multi', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_pkg_imported(self):
        """test provides pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refsymbols = {
            'pkg_imported.argh': '',
            'pkg_imported.FooBar': '',
            'pkg_imported.FooBar.__init__': '',
            'pkg_imported.FooBar.new_data': '',
            'pkg_imported.FooBar.fullname': '',
            'pkg_imported.FooBar.hello': '',
        }
        syms = _load_py_symbols('pkg_imported', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_depends_import_simple(self):
        """test dependent imports with simple package with no import"""
        pkgfiles = ['simple/__init__.py']
        refimports = set()
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_import_multi(self):
        """test dependent import with multiple modules"""
        pkgfiles = [
            'multi/__main__.py',
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refimports = set()
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_import_pkg_imported(self):
        """test dependent import with pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refimports = {
            'simple.main_dummy_fn',
            'simple.MainData.__init__',
            'simple.MainData.disclose_private',
        }
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_launcher(self):
        """test dependent imports with simple package with no import"""
        pkgfiles = ['launcher']
        refimports = {
            'multi.__main__',
            'pkg_resources.load_entry_point',
        }
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)
