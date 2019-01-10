# @mindmaze_header@
import unittest

from mm_version import Version
from distutils.version import LooseVersion

class TestVersionClass(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_simple(self):
        'smoke test'
        self.assertLess(Version("1"), Version("2"))
        self.assertEqual(Version("1"), Version("1"))
        self.assertGreater(Version("2"), Version("1"))

    def test_xyz_format(self):
        'x.y.z version format'
        self.assertLess(Version("1.0.0"), Version("2.0.0"))
        self.assertGreater(Version("2.0.0"), Version("1.0.0"))
        self.assertLess(Version("1.2.3"), Version("5.1.0"))
        self.assertEqual(Version("1.2.3"), Version("1.2.3"))

    def test_date_format(self):
        'date version format'
        self.assertLess(Version("16.04"), Version("18.04"))
        self.assertLess(Version("16.04"), Version("18.04"))
        self.assertGreater(Version("16.10"), Version("16.9"))
        self.assertLess(Version("01.10"), Version("10.9"))

    def test_other_formats(self):
        'test various mix formats'
        self.assertLess(Version("v1.2.3"), Version("v2.3.4"))
        self.assertLess(Version("1"), Version("1.1"))
        self.assertLess(Version("1.2"), Version("1.2.1"))
        self.assertLess(Version("v01.9.0"), Version("v1.90.0"))
        self.assertGreater(Version("vv1.9.0"), Version("v01.9.0"))

    def test_no_digits(self):
        'ensure there are no exception inherited from LooseVersion'
        self.assertLess(Version("a"), Version("b"))
        self.assertEqual(Version("any"), Version("october"))
        self.assertGreater(Version("september"), Version("october"))

    def test_wildcard_version(self):
        'test version wildcards'
        self.assertLess(Version("1.0.0"), Version("any"))
        self.assertLess(Version("any"), Version("1.0.0"))

    def test_nodigits(self):
        'test LooseVersion issue when one version does not have any digits'
        self.assertLess(Version("any"), Version("nodigits"))
        self.assertLess(Version("any"), Version("nodigits"))
        self.assertLess(Version("1.0.0"), Version("nodigits"))
        self.assertGreater(Version("nodigits"), Version("1.0.0"))

    def test_prohibited(self):
        'test that underscores are rejected by the version class'
        with self.assertRaises(SyntaxError):
            Version('1_2.3')
