# @mindmaze_header@

import unittest
from glob import glob
from os.path import dirname, abspath, join
from typing import Set

from mmpack_build.package_info import PackageInfo
from mmpack_build.hook_python import (_gen_py_importname, _gen_pysymbols,
                                      _gen_pydepends, _FILENAME_GENERATOR)


_testdir = dirname(abspath(__file__))
_sitedir = join(_testdir, 'pydata')


def _prefix_sitedir(files: Set[str]) -> Set[str]:
    return {_sitedir + '/' + f for f in files}


def _load_py_symbols(pkgfiles: Set[str]) -> Set[str]:
    return _gen_pysymbols(_prefix_sitedir(pkgfiles), [_sitedir])


def _get_py_depends(pkgfiles: Set[str]) -> Set[str]:
    pkg = PackageInfo('test_pkg')
    pkg.files = _prefix_sitedir(pkgfiles)
    used_symbols = _gen_pydepends(pkg, [_sitedir])
    return used_symbols


def _get_py_dispatch(pkgfiles: Set[str]) -> Set[str]:
    return _gen_py_importname(_prefix_sitedir(pkgfiles), [_sitedir])


class TestPythonHook(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        _FILENAME_GENERATOR.reset('.')

    def test_provides_bare_module(self):
        """test provides module without package folder"""
        pkgfiles = ['bare.py']
        refsymbols = {'bare': {
            'bare.A_CLASS',
            'bare.EXPORTED_LIST',
            'bare.THE_ANSWER',
            'bare.main_dummy_fn',
            'bare.MainData',
            'bare.MainData.a_class_attr',
            'bare.MainData.__init__',
            'bare.MainData.data1',
            'bare.MainData.fullname',
            'bare.MainData.disclose_private',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_simple(self):
        """test provides no import"""
        pkgfiles = ['simple/__init__.py']
        refsymbols = {'simple': {
            'simple.A_CLASS',
            'simple.EXPORTED_LIST',
            'simple.THE_ANSWER',
            'simple.main_dummy_fn',
            'simple.MainData',
            'simple.MainData.a_class_attr',
            'simple.MainData.__init__',
            'simple.MainData.data1',
            'simple.MainData.fullname',
            'simple.MainData.disclose_private',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_multi(self):
        """test provides pkg with multiple modules"""
        pkgfiles = [
            'multi/__main__.py',
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refsymbols = {'multi': {
            'multi.foo.DummyData.a_number',
            'multi.foo.DummyData.an_attr',
            'multi.foo.DummyData.v1',
            'multi.foo.DummyData.__init__',
            'multi.foo.DummyData.useless_method',
            'multi.foo.DummyData',
            'multi.foo.Employee',
            'multi.foo.Employee2',
            'multi.foo.MainData',
            'multi.foo.MainData.a_class_attr',
            'multi.foo.MainData.__init__',
            'multi.foo.MainData.data1',
            'multi.foo.MainData.fullname',
            'multi.foo.MainData.disclose_private',
            'multi.foo.somefunc',
            'multi.foo.A_CLASS',
            'multi.foo.EXPORTED_LIST',
            'multi.foo.THE_ANSWER',
            'multi.foo.main_dummy_fn',
            'multi.foo.utils',
            'multi.FooBar',
            'multi.FooBar.__init__',
            'multi.FooBar.fullname',
            'multi.FooBar.new_data',
            'multi.FooBar.hello',
            'multi.bar',
            'multi.bar.print_hello',
            'multi.bar.Bar',
            'multi.bar.Bar.__init__',
            'multi.bar.Bar.drink',
            'multi.bar.A_BAR',
            'multi.bar.Employee2.id2',
            'multi.bar.NamedTuple',
            'multi.bar.Employee2.name2',
            'multi.bar.Employee',
            'multi.bar.Employee2',
            'multi.MainData',
            'multi.somefunc',
            'multi.__main__',
            'multi.__main__.print_hello',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_multi2(self):
        """test provides pkg with multiple modules"""
        pkgfiles = [
            'multi2/__main__.py',
            'multi2/__init__.py',
            'multi2/foo.py',
            'multi2/bar.py',
        ]
        refsymbols = {'multi2': {
            'multi2.foo.DummyData.a_number',
            'multi2.foo.DummyData.an_attr',
            'multi2.foo.DummyData.v1',
            'multi2.foo.DummyData.__init__',
            'multi2.foo.DummyData.useless_method',
            'multi2.foo.DummyData',
            'multi2.foo.MainData',
            'multi2.foo.MainData.a_class_attr',
            'multi2.foo.MainData.__init__',
            'multi2.foo.MainData.data1',
            'multi2.foo.MainData.fullname',
            'multi2.foo.MainData.disclose_private',
            'multi2.foo.somefunc',
            'multi2.foo.A_CLASS',
            'multi2.foo.EXPORTED_LIST',
            'multi2.foo.THE_ANSWER',
            'multi2.foo.main_dummy_fn',
            'multi2.foo.utils',
            'multi2.FooBar',
            'multi2.FooBar.__init__',
            'multi2.FooBar.fullname',
            'multi2.FooBar.new_data',
            'multi2.FooBar.hello',
            'multi2.bar.print_hello',
            'multi2.bar.Bar',
            'multi2.bar.Bar.__init__',
            'multi2.bar.Bar.drink',
            'multi2.bar.A_BAR',
            'multi2.MainData',
            'multi2.somefunc',
            'multi2.__main__',
            'multi2.__main__.print_hello',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_pkg_imported(self):
        """test provides pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refsymbols = {'pkg_imported': {
            'pkg_imported.argh',
            'pkg_imported.bar',
            'pkg_imported.main',
            'pkg_imported.main_dummy_fn',
            'pkg_imported.somefunc',
            'pkg_imported.Employee',
            'pkg_imported.Employee2',
            'pkg_imported.FooBar',
            'pkg_imported.FooBar.__init__',
            'pkg_imported.FooBar.new_data',
            'pkg_imported.FooBar.fullname',
            'pkg_imported.FooBar.hello',
            'pkg_imported.MainData',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_ns_pkg(self):
        """test provides namespace package"""
        pkgfiles = ['nspace/pkg_a/__init__.py']
        refsymbols = {'nspace.pkg_a': {
            'nspace.pkg_a.NSpacePkgAData',
            'nspace.pkg_a.NSpacePkgAData.__init__',
            'nspace.pkg_a.NSpacePkgAData.public',
            'nspace.pkg_a.NSpacePkgAData.show',
            'nspace.pkg_a.nspace_pkga_func',
        }}
        syms = _load_py_symbols(pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_depends_import_simple(self):
        """test dependent imports with simple package with no import"""
        pkgfiles = ['simple/__init__.py']
        refimports = {}
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
        refimports = {}
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_import_pkg_imported(self):
        """test dependent import with pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refimports = {
            'multi': {
                'multi.somefunc',
                'multi.bar.Employee',
                'multi.bar.Employee.name',
                'multi.bar.Employee2',
            },
            'simple': {
                'simple.main_dummy_fn',
                'simple.MainData',
                'simple.MainData.__init__',
                'simple.MainData.disclose_private',
            },
            'nspace.pkg_a': {
                'nspace.pkg_a.NSpacePkgAData',
                'nspace.pkg_a.NSpacePkgAData.show',
            }
        }
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_depends_launcher(self):
        """test dependent imports with simple package with no import"""
        pkgfiles = ['launcher']
        refimports = {
            'multi': {'multi.__main__.main'},
            'pkg_resources': {'pkg_resources.load_entry_point'},
        }
        imports = _get_py_depends(pkgfiles)
        self.assertEqual(imports, refimports)

    def test_dispatch_bare(self):
        pkgfiles = [
            'bare.py',
        ]
        refdispatch = {
            'bare': _prefix_sitedir({
                'bare.py',
            }),
        }
        dispatch = _get_py_dispatch(pkgfiles)
        self.assertEqual(dispatch, refdispatch)

    def test_dispatch_pkg_imported(self):
        pkgfiles = [
            'pkg_imported/__init__.py',
        ]
        refdispatch = {
            'pkg_imported': _prefix_sitedir({
                'pkg_imported/__init__.py',
            }),
        }
        dispatch = _get_py_dispatch(pkgfiles)
        self.assertEqual(dispatch, refdispatch)

    def test_dispatch_multi(self):
        pkgfiles = [
            'multi/__main__.py',
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refdispatch = {
            'multi': _prefix_sitedir({
                'multi/__main__.py',
                'multi/__init__.py',
                'multi/foo.py',
                'multi/bar.py',
            }),
        }
        dispatch = _get_py_dispatch(pkgfiles)
        self.assertEqual(dispatch, refdispatch)

    def test_dispatch_combined(self):
        pkgfiles = [
            'bare.py',
            'multi/__main__.py',
            'multi/__init__.py',
            'multi/foo.py',
            'multi/bar.py',
        ]
        refdispatch = {
            'multi': _prefix_sitedir({
                'multi/__main__.py',
                'multi/__init__.py',
                'multi/foo.py',
                'multi/bar.py',
            }),
            'bare': _prefix_sitedir({
                'bare.py',
            }),
        }
        dispatch = _get_py_dispatch(pkgfiles)
        self.assertEqual(dispatch, refdispatch)

    def test_dispatch_ns_pkg(self):
        """test provides namespace package"""
        pkgfiles = ['nspace/pkg_a/__init__.py']
        refdispatch = {
            'nspace.pkg_a': _prefix_sitedir({
                'nspace/pkg_a/__init__.py',
            }),
        }
        dispatch = _get_py_dispatch(pkgfiles)
        self.assertEqual(dispatch, refdispatch)
