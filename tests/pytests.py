import os
import unittest

from tap import TAPTestRunner


if __name__ == '__main__':
    tests_dir = os.path.dirname(os.path.abspath(__file__))
    case_filter = os.environ.get('PY_RUN_CASE', '')
    pattern = 'test_*{0}*.py'.format(case_filter)

    loader = unittest.TestLoader()
    tests = loader.discover(tests_dir, pattern=pattern)

    runner = TAPTestRunner()
    runner.set_stream(True)
    runner.set_outdir('.')
    runner.set_format('{method_name} : {short_description}')
    runner.run(tests)
