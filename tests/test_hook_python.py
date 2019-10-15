# @mindmaze_header@

import unittest
from glob import glob
from os.path import dirname, abspath, join
from typing import Set

from mmpack_build.base_hook import PackageInfo
from mmpack_build.hook_python import _gen_pysymbols


_testdir = dirname(abspath(__file__))
_sitedir = join(_testdir, 'pydata')


def _load_py_symbols(name: str, pkgfiles: Set[str]) -> Set[str]:
    pkg = PackageInfo('test_pkg')
    pkg.files = {join(_sitedir, f) for f in pkgfiles}

    return _gen_pysymbols(name, pkg, _sitedir)


class TestPythonProvides(unittest.TestCase):

    def test_provides_bare_module(self):
        """test provides module without package folder"""
        pkgfiles = ['bare.py']
        refsymbols = {
            'A_CLASS',
            'EXPORTED_LIST',
            'THE_ANSWER',
            'main_dummy_fn',
            'MainData',
            'MainData.a_class_attr',
            'MainData.__init__',
            'MainData.data1',
            'MainData.fullname',
            'MainData.disclose_private',
        }
        syms = _load_py_symbols('bare', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_simple(self):
        """test provides no import"""
        pkgfiles = ['simple/__init__.py']
        refsymbols = {
            'A_CLASS',
            'EXPORTED_LIST',
            'THE_ANSWER',
            'main_dummy_fn',
            'MainData',
            'MainData.a_class_attr',
            'MainData.__init__',
            'MainData.data1',
            'MainData.fullname',
            'MainData.disclose_private',
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
            'argh',
            'main_dummy_fn',
            'MainData',
            'MainData.a_class_attr',
            'MainData.__init__',
            'MainData.data1',
            'MainData.fullname',
            'MainData.disclose_private',
            'FooBar',
            'FooBar.a_class_attr',
            'FooBar.__init__',
            'FooBar.data1',
            'FooBar.fullname',
            'FooBar.new_data',
            'FooBar.disclose_private',
            'FooBar.hello',
            'bar.print_hello',
            'bar.Bar',
            'bar.Bar.__init__',
            'bar.Bar.drink',
            'bar.A_BAR',
        }
        syms = _load_py_symbols('multi', pkgfiles)
        self.assertEqual(syms, refsymbols)

    def test_provides_pkg_imported(self):
        """test provides pkg importing another package"""
        pkgfiles = ['pkg_imported/__init__.py']
        refsymbols = {
            'argh',
            'FooBar',
            'FooBar.__init__',
            'FooBar.new_data',
            'FooBar.fullname',
            'FooBar.hello',
        }
        syms = _load_py_symbols('pkg_imported', pkgfiles)
        self.assertEqual(syms, refsymbols)
