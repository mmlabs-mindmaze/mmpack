# @mindmaze_header@
import os
import unittest

from mmpack_build.src_package import SrcPackage
from mmpack_build.mm_version import Version


class TestSrcPackageClass(unittest.TestCase):
    def setUp(self):
        # Create empty dummy file
        open('empty_file', 'wb')

    def tearDown(self):
        # Delete dummy file
        os.remove('empty_file')

    def test_package_simple(self):
        """
        smoke test
        """
        specfile = os.path.dirname(os.path.abspath(__file__)) + '/specfiles' + '/simple.yaml'
        test_pkg = SrcPackage(specfile, 'dummy_tag', 'empty_file')

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
        test_pkg = SrcPackage(specfile, 'dummy_tag', 'empty_file')

        # test the general section values
        self.assertEqual(test_pkg.name, 'full')
        self.assertEqual(test_pkg.version, Version('1.0.0'))
        self.assertEqual(test_pkg.maintainer, 'mmpack.test@mindmaze.ch')
        self.assertEqual(test_pkg.url, 'ssh://git@intranet.mindmaze.ch:7999/~mmpack.test/full.git')
        self.assertEqual(test_pkg.description,
                         "This is the fullest mmpack specfile possible.\n")
        self.assertEqual(test_pkg.src_hash,
                         'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')

        self.assertEqual(test_pkg.build_options, '-D_WITH_DUMMY_DEFINE=1')
        self.assertEqual(len(test_pkg.build_depends), 2)
        ref_build_depends = ['libmyotherlib-devel', 'libsomeotherlib-devel']
        self.assertEqual(sorted(test_pkg.build_depends), sorted(ref_build_depends))

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
        pkgs = test_pkg._ventilate_custom_packages()
        test_pkg._create_binpkgs_from_pkginfos(pkgs)

        ref_pkg_list_name = ['full', 'custom-package']
        test_pkg_list_name = test_pkg._packages.keys()
        self.assertEqual(len(test_pkg._packages), 2)
        self.assertEqual(sorted(ref_pkg_list_name), sorted(test_pkg_list_name))

        # test the customized packages further
        full = test_pkg._packages['full']
        custom = test_pkg._packages['custom-package']

        self.assertEqual(len(full._dependencies['depends']), 2)
        self.assertRegex(full.description,
                         r'This is the fullest mmpack specfile possible.\n\nThis should follow.*')

        self.assertEqual(len(custom._dependencies['sysdepends']), 2)
        self.assertEqual(len(custom._dependencies['depends']), 2)
        self.assertRegexpMatches(custom.description,
                                 r'This should overload .*')
