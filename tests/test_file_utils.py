# @mindmaze_header@

import os
import unittest

from mmpack_build.file_utils import filetype



class TestFileUtils(unittest.TestCase):

    def test_filetype_shebang(self):
        """
        test that filetype return the right intepreter
        """
        cases = [
            ('#!/bin/bash', 'bash'),
            ('#!/bin/sh', 'sh'),
            ('#!  /bin/bash', 'bash'),
            ('#!  /bin/sh', 'sh'),
            ('#!/bin/bash -xe', 'bash'),
            ('#!/usr/bin/python', 'python'),
            ('#!/usr/bin/python3', 'python3'),
            ('#!/usr/bin/env python', 'python'),
            ('#!/usr/bin/env python3', 'python3'),
            ('#!/usr/bin/env python -e -x', 'python'),
            ('#!/usr/bin/env python3 ds', 'python3'),
            ('#! /foo/bar ds', 'bar'),
        ]

        for shebang, exp_interpreter in cases:
            testfile = 'test.file'

            content = shebang + '\ndummy data\n\rfilling stuff'
            open(testfile, 'wt').write(content)

            ftype = filetype(testfile)

            os.unlink(testfile)

            self.assertEqual(ftype, exp_interpreter)
