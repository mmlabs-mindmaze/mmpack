#!/usr/bin/env python3

import copy
import os
import sys
import unittest
from pathlib import Path
from importlib.util import spec_from_file_location, module_from_spec


class TapTestResult(unittest.TestResult):
    def __init__(self, stream):
        super(TapTestResult, self).__init__(self)
        self.stream = stream

    def _print_tap_result(self, result, test, directive=None):
        dirstr = ""
        if directive:
            dirstr = " # " + directive
        description = "{} : {}".format(test.id(), test.shortDescription())
        self.stream.write("{} {} {}{}\n".format(result,
                                                self.testsRun,
                                                description,
                                                dirstr))
        self.stream.flush()

    def addSuccess(self, test):
        super(TapTestResult, self).addSuccess(test)
        self._print_tap_result("ok", test)

    def addError(self, test, err):
        super(TapTestResult, self).addError(test, err)
        sys.stderr.write(self.errors[-1][1] + "\n")
        self._print_tap_result("not ok", test)

    def addFailure(self, test, err):
        super(TapTestResult, self).addFailure(test, err)
        sys.stderr.write(self.failures[-1][1] + "\n")
        self._print_tap_result("not ok", test)

    def addSkip(self, test, reason):
        super(TapTestResult, self).addSkip(test, reason)
        self._print_tap_result("ok", test, "SKIP " + reason)

    def addExpectedFailure(self, test, err):
        super(TapTestResult, self).addExpectedFailure(test, err)
        self._print_tap_result("ok", test)

    def addUnexpectedSuccess(self, test):
        super(TapTestResult, self).addUnexpectedSuccess(test)
        self._print_tap_result("not ok", test)


class TapTestRunner(unittest.TextTestRunner):
    def __init__(self, output = sys.stdout):
        super().__init__(self)
        self.stream = output

    def run(self, test):
        # Write TAP header. The testplan does not have to be at the beginning,
        # but it is better since this way the consumer knows how many have not
        # been run. To see specs of TAP:
        # https://testanything.org/tap-specification.html
        num_tests = test.countTestCases()
        self.stream.write("TAP version 13\n1..{}\n".format(num_tests))
        self.stream.flush()

        result = TapTestResult(self.stream)
        unittest.registerResult(result)
        test(result)
        unittest.removeResult(result)
        return result


def prepare_import():
    srcpath = Path(__file__).parent.parent / 'src'

    module_path = str(srcpath.resolve() / 'mmpack-build' / '__init__.py')
    module_name = 'mmpack_build'
    spec = spec_from_file_location(module_name, module_path)
    module = module_from_spec(spec)
    sys.modules[module_name] = module


if __name__ == '__main__':
    prepare_import()

    tests_dir = os.path.dirname(os.path.abspath(__file__))
    case_filter = os.environ.get('PY_RUN_CASE', '')
    pattern = 'test_*{0}*.py'.format(case_filter)

    loader = unittest.TestLoader()

    # python unittest discover modifies sys.path
    # (See: https://bugs.python.org/issue24247)
    # to prevent path mixup between the package being tested and an eventual
    # installed package, prevent any such change
    path_backup = copy.deepcopy(sys.path)
    tests = loader.discover(tests_dir, pattern=pattern)
    sys.path = path_backup

    runner = TapTestRunner()
    rv = runner.run(tests)
    if (len(rv.errors) + len(rv.failures) + len(rv.unexpectedSuccesses)) != 0:
        exit(-1)
    exit(0)
