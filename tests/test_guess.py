# @mindmaze_header@
import unittest

from mmpack_build.mmpack_guess import _common_lineseq, _common_substring

DESC_PKG1 = \
"optional static typing for Python\n"\
"Add type annotations to your Python programs, and use mypy to type check them. "\
"Mypy is essentially a Python linter on steroids, and it can catch many "\
"programming errors by analyzing your program, without actually having to run "\
"it. Mypy has a powerful type system with features such as type inference, "\
"gradual typing, generics and union types.\n"\
"This package provides the command-line interface."

DESC_PKG2 = \
"documentation for mypy\n"\
"Add type annotations to your Python programs, and use mypy to type check them. "\
"Mypy is essentially a Python linter on steroids, and it can catch many "\
"programming errors by analyzing your program, without actually having to run "\
"it. Mypy has a powerful type system with features such as type inference, "\
"gradual typing, generics and union types.\n"\
"This package provides the documentation."

EXPECTED_DESC = \
"Add type annotations to your Python programs, and use mypy to type check them. "\
"Mypy is essentially a Python linter on steroids, and it can catch many "\
"programming errors by analyzing your program, without actually having to run "\
"it. Mypy has a powerful type system with features such as type inference, "\
"gradual typing, generics and union types."


class TestGuess(unittest.TestCase):
    def test_common_substring(self):
        cases = [
            (['1.7.7'], '1.7.7'),
            (['1.7.7-binnmu1', '1.7.7'], '1.7.7'),
        ]
        for case in cases:
            self.assertEqual(_common_substring(case[0]), case[1])

    def test_common_lineseq(self):
        cases = [
            ([DESC_PKG1], DESC_PKG1),
            ([DESC_PKG1, DESC_PKG2], EXPECTED_DESC),
        ]
        for case in cases:
            self.assertEqual(_common_lineseq(case[0]), case[1])
