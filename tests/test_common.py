# @mindmaze_header@
import unittest

from os import makedirs, getcwd, chdir
from os.path import dirname, abspath
from shutil import rmtree

from mmpack_build.common import list_files, pushdir, popdir


REF_FILELIST = [
    'adir/bdir/one',
    'adir/cfile',
    'adir/dfile',
    'afile',
    'bdir/bdir/one',
    'bdir/cfile',
    'bdir/dfile',
    'bfile',
]

TEST_TREE_ROOT = 'test_tree/root'


class TestFileList(unittest.TestCase):
    def setUp(self):
        makedirs(TEST_TREE_ROOT, exist_ok=True)
        pushdir(TEST_TREE_ROOT)

        for path in REF_FILELIST:
            # Create dummy file located at path
            dirpath = dirname(path)
            if dirpath:
                makedirs(dirpath, exist_ok=True)
            open(path, 'w')

        popdir()
        pushdir('.')

    def tearDown(self):
        popdir()
        rmtree(TEST_TREE_ROOT)

    def test_list_files_multiple_pathcomp(self):
        """
        test list_files on topdir with multiple component
        """
        filelist = list_files(TEST_TREE_ROOT)
        self.assertEqual(sorted(filelist), REF_FILELIST)

    def test_list_files_single_pathcomp(self):
        """
        test list_files on topdir with single component
        """
        chdir('test_tree')
        filelist = list_files('root')
        self.assertEqual(sorted(filelist), REF_FILELIST)

    def test_list_files_currdir(self):
        """
        test list_files on topdir with single component
        """
        chdir(TEST_TREE_ROOT)
        filelist = list_files('.')
        self.assertEqual(sorted(filelist), REF_FILELIST)

    def test_list_files_abspath(self):
        """
        test list_files on topdir asabsolute path
        """
        filelist = list_files(abspath(TEST_TREE_ROOT))
        self.assertEqual(sorted(filelist), REF_FILELIST)

