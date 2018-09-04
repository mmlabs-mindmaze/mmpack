# @mindmaze_header@
import unittest

# do this before importing from mmpack
import os
import sys
import mmpack
for path in mmpack.__path__:
    sys.path.insert(0, path)

# import as if from mmpack package
from package import Package

class TestPackageClass(unittest.TestCase):
    def test_package_simple(self):
        'smoke test'
        specfile = os.path.dirname(os.path.abspath(__file__)) + '/specfiles' + '/simple.yaml'
        test_pkg = Package('dummy.maintainer@mindmaze.ch')
        test_pkg.load_specfile(specfile)
        test_pkg.parse_specfile()

        self.assertEqual(test_pkg.name, 'simple')
        self.assertEqual(test_pkg.version, '1.0.0')
        self.assertEqual(test_pkg.maintainer, 'mmpack.test@mindmaze.ch')
        self.assertEqual(test_pkg.url, 'ssh://git@intranet.mindmaze.ch:7999/~mmpack.test/simple.git')
        self.assertRegexpMatches(test_pkg.description,
                                 r'This is the simplest mmpack specfile possible.*')

    def test_package_full(self):
        'the full spec parsing'
        specfile = os.path.dirname(os.path.abspath(__file__)) + '/specfiles' + '/full.yaml'
        test_pkg = Package('dummy.maintainer@mindmaze.ch')
        test_pkg.load_specfile(specfile)
        test_pkg.parse_specfile()

        # test the general section values
        self.assertEqual(test_pkg.name, 'full')
        self.assertEqual(test_pkg.version, '1.0.0')
        self.assertEqual(test_pkg.maintainer, 'mmpack.test@mindmaze.ch')
        self.assertEqual(test_pkg.url, 'ssh://git@intranet.mindmaze.ch:7999/~mmpack.test/full.git')
        self.assertEqual(test_pkg.description,
                         "This is the fullest mmpack specfile possible.\n")

        self.assertEqual(test_pkg.build_options, '-D_WITH_DUMMY_DEFINE=1')
        self.assertEqual(len(test_pkg.build_depends), 2)
        ref_build_depends = ['libmyotherlib-devel', 'libsomeotherlib-devel']
        self.assertEqual(sorted(test_pkg.build_depends), sorted(ref_build_depends))

        # ignore list is not tested here: they are removed from the install list
        # which means that doing a full install is necessary

        # test the overloaded and custom packages
        # This does not include the implicit packages
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
