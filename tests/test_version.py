# @mindmaze_header@
import unittest

# do this before importing from mmpack
import sys
import mmpack
for path in mmpack.__path__:
    sys.path.insert(0, path)

# import as if from mmpack package
from version import Version

class TestPlaceHolder(unittest.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_dummy_1(self):
        'dummy test 1'
        self.assertEqual(12, 12)

    def test_dummy_2(self):
        'dummy test 2'
        self.assertEqual('aaa', 'aaa')


