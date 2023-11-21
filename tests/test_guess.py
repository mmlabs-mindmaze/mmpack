# @mindmaze_header@
import unittest

from mmpack_build.mmpack_guess import _common_substring


class TestGuess(unittest.TestCase):
    def test_common_substring(self):
        cases = [
            (['1.7.7'], '1.7.7'),
            (['1.7.7-binnmu1', '1.7.7'], '1.7.7'),
        ]
        for case in cases:
            self.assertEqual(_common_substring(case[0]), case[1])
