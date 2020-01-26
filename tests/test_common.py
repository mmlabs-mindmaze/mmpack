# @mindmaze_header@
import unittest

from os import makedirs, getcwd, chdir
from os.path import dirname, abspath
from shutil import rmtree

from mmpack_build.common import list_files, parse_soname, shlib_keyname


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

REF_FILELIST_WITHDIRS = [
    'adir',
    'adir/bdir',
    'adir/bdir/one',
    'adir/cfile',
    'adir/dfile',
    'afile',
    'bdir',
    'bdir/bdir',
    'bdir/bdir/one',
    'bdir/cfile',
    'bdir/dfile',
    'bfile',
]

TEST_TREE_ROOT = 'test_tree/root'


REF_SONAME_DATA = [
    ('libfoo.so.1', 'libfoo', '1', 'libfoo1'),
    ('libfoo-1.dll', 'libfoo', '1', 'libfoo1'),
    ('libfoo.so.1.2', 'libfoo', '1.2', 'libfoo1.2'),
    ('libfoo-1.2.dll', 'libfoo', '1.2', 'libfoo1.2'),
    ('libfoo1.2.so.3', 'libfoo1.2', '3', 'libfoo1.2-3'),
    ('libfoo1.2-3.dll', 'libfoo1.2', '3', 'libfoo1.2-3'),
]


class TestFileList(unittest.TestCase):
    abs_testdir = None

    @classmethod
    def setUpClass(cls):
        cls.abs_testdir = getcwd()
        makedirs(TEST_TREE_ROOT, exist_ok=True)
        chdir(TEST_TREE_ROOT)

        for path in REF_FILELIST:
            # Create dummy file located at path
            dirpath = dirname(path)
            if dirpath:
                makedirs(dirpath, exist_ok=True)
            open(path, 'w')

        chdir(cls.abs_testdir)

    @classmethod
    def tearDownClass(cls):
        rmtree(TEST_TREE_ROOT)

    def tearDown(self):
        chdir(self.abs_testdir)

    def test_list_files_with_dirs(self):
        """
        test list_files on topdir with multiple component
        """
        filelist = list_files(TEST_TREE_ROOT)
        self.assertEqual(filelist, REF_FILELIST_WITHDIRS)

    def test_list_files_multiple_pathcomp(self):
        """
        test list_files on topdir with multiple component
        """
        filelist = list_files(TEST_TREE_ROOT, exclude_dirs=True)
        self.assertEqual(filelist, REF_FILELIST)

    def test_list_files_single_pathcomp(self):
        """
        test list_files on topdir with single component
        """
        chdir('test_tree')
        filelist = list_files('root', exclude_dirs=True)
        self.assertEqual(filelist, REF_FILELIST)

    def test_list_files_currdir(self):
        """
        test list_files on topdir with single component
        """
        chdir(TEST_TREE_ROOT)
        filelist = list_files('.', exclude_dirs=True)
        self.assertEqual(filelist, REF_FILELIST)

    def test_list_files_abspath(self):
        """
        test list_files on topdir asabsolute path
        """
        filelist = list_files(abspath(TEST_TREE_ROOT), exclude_dirs=True)
        self.assertEqual(filelist, REF_FILELIST)

    def test_soname_parsing(self):
        """
        test parse_soname()
        """
        for ref_soname, ref_name, ref_version, _ in REF_SONAME_DATA:
            name, version = parse_soname(ref_soname)
            self.assertEqual(name, ref_name)
            self.assertEqual(version, ref_version)

    def test_shlib_name(self):
        """
        test shlib_keyname()
        """
        for ref_soname, _, _, ref_pkgname in REF_SONAME_DATA:
            pkgname = shlib_keyname(ref_soname)
            self.assertEqual(pkgname, ref_pkgname)
