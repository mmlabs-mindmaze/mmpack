# @mindmaze_header@

import os
import unittest

from mmpack_build.mmpack_guess import _guess_description


class TestDescriptionGuess(unittest.TestCase):
    """
    test description extraction from markdown-formatted READMEs
    """
    def test_guess_description_simple(self):
        """
        smoke test
        """
        readme = '''# dummy

                    content

                    some other content to ignore
                 '''
        desc =_guess_description(readme).strip()
        self.assertEqual(desc, 'content')

    def test_guess_description_shields(self):
        """
        smoke test: skip paragraph if starting with invalid char
        """
        readme = '''# dummy

                    # skipme

                    [skipme]

                    ======= skipme

                    content

                    some other content to ignore
                 '''
        desc =_guess_description(readme).strip()
        self.assertEqual(desc, 'content')


