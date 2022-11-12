# @mindmaze_header@
import os
import unittest
import tarfile
from typing import Dict
from shutil import rmtree

from mmpack_build.common import sha256sum
from mmpack_build.src_package import SrcPackage
from mmpack_build.mm_version import Version
from mmpack_build.package_info import DispatchData


_TEST_SRCPKG = 'testsrc.tar.xz'
_TESTS_DATA_DIR = os.environ.get('TESTSDIR', '.') + '/tmp-data'


def _create_test_srcpkg(content: Dict[str, str]):
    tar = tarfile.open(_TEST_SRCPKG, 'w:xz')
    for arcname, filename in content.items():
        tar.add(filename, arcname)
    tar.close()


class TestSrcPackageClass(unittest.TestCase):
    abs_testdir = '.'

    @classmethod
    def setUpClass(cls):
        cls.abs_testdir = os.getcwd()
        os.makedirs(_TESTS_DATA_DIR, exist_ok=True)
        os.chdir(_TESTS_DATA_DIR)

    @classmethod
    def tearDownClass(cls):
        os.chdir(cls.abs_testdir)
        rmtree(_TESTS_DATA_DIR, ignore_errors=True)

    def tearDown(self):
        # Delete test src pkg
        try:
            os.remove(_TEST_SRCPKG)
        except FileNotFoundError:
            pass

    def test_package_simple(self):
        """
        smoke test
        """
        specfile = os.path.dirname(os.path.abspath(__file__)) + '/specfiles' + '/simple.yaml'
        _create_test_srcpkg({'./mmpack/specs': specfile})
        test_pkg = SrcPackage(_TEST_SRCPKG, 'dummy_tag')

        self.assertEqual(test_pkg.name, 'simple')
        self.assertEqual(test_pkg.version, Version('1.0.0'))
        self.assertEqual(test_pkg.maintainer, 'mmpack.test@mindmaze.ch')
        self.assertEqual(test_pkg.url, 'ssh://git@intranet.mindmaze.ch:7999/~mmpack.test/simple.git')
        self.assertRegexpMatches(test_pkg.description,
                                 r'This is the simplest mmpack specfile possible.*')

    def test_package_full(self):
        """
        the full spec parsing
        """
        specfile = os.path.dirname(os.path.abspath(__file__)) + '/specfiles' + '/full.yaml'
        _create_test_srcpkg({'./mmpack/specs': specfile})
        test_pkg = SrcPackage(_TEST_SRCPKG, 'dummy_tag')

        # test the general section values
        self.assertEqual(test_pkg.name, 'full')
        self.assertEqual(test_pkg.version, Version('1.0.0'))
        self.assertEqual(test_pkg.maintainer, 'mmpack.test@mindmaze.ch')
        self.assertEqual(test_pkg.url, 'ssh://git@intranet.mindmaze.ch:7999/~mmpack.test/full.git')
        self.assertEqual(test_pkg.description,
                         "This is the fullest mmpack specfile possible.")
        self.assertEqual(test_pkg.src_hash, sha256sum(_TEST_SRCPKG))

        build_depends = test_pkg._specs.get('build-depends', [])
        self.assertEqual(test_pkg.build_options, '-D_WITH_DUMMY_DEFINE=1')
        self.assertEqual(len(build_depends), 2)
        ref_build_depends = ['libmyotherlib-devel', 'libsomeotherlib-devel']
        self.assertEqual(sorted(build_depends), sorted(ref_build_depends))

        test_pkg.install_files_set = {
            '/full-binary-package-file-1',
            '/full-binary-package-file-2',
            '/full-binary-package-file-3',
            'foo/full-binary-package-file-1',
            'bar/full-binary-package-file-2',
            'baz/full-binary-package-file-3',
            'bar/custom-package-file-1',
            'bar/custom-package-file-2',
            'bar/custom-package-regex/1',
            'foo/custom-package-regex/2',
            'bar/custom-package-regex/3',
        }

        # ignore list is not tested here: they are removed from the install list
        # which means that doing a full install is necessary

        # test the overloaded and custom packages
        # This does not include the implicit packages
        data = DispatchData(test_pkg.install_files_set)
        test_pkg._ventilate_custom_packages(data)
        test_pkg._create_binpkgs_from_dispatch(data)

        ref_pkg_list_name = ['full', 'custom-package']
        test_pkg_list_name = test_pkg._packages.keys()
        self.assertEqual(len(test_pkg._packages), 2)
        self.assertEqual(sorted(ref_pkg_list_name), sorted(test_pkg_list_name))

        # test the customized packages further
        full = test_pkg._packages['full']
        custom = test_pkg._packages['custom-package']

        self.assertEqual(len(full._dependencies['depends']), 2)
        self.assertRegex(full.description,
                         r'This is the fullest mmpack specfile possible.\nThis should follow.*')

        self.assertEqual(len(custom._dependencies['sysdepends']), 2)
        self.assertEqual(len(custom._dependencies['depends']), 4)
        self.assertRegexpMatches(custom.description,
                                 r'This should overload .*')
    
    def test_package_expand(self):
        """
        smoke test
        """
        testdir = os.path.dirname(os.path.abspath(__file__))
        specfile = testdir + '/specfiles/expand.yaml'
        versionfile = testdir + '/specfiles/VERSION_FILE'
        _create_test_srcpkg({
            './mmpack/specs': specfile,
            './VERSION_FILE': versionfile
        })
        test_pkg = SrcPackage(_TEST_SRCPKG, 'dummy_tag')

        self.assertEqual(test_pkg.name, 'expand')
        self.assertEqual(test_pkg.version, Version('0.1.2'))
