# @mindmaze_header@
import unittest

from collections import namedtuple
from os import makedirs, getcwd, chdir
from os.path import dirname, abspath
from shutil import rmtree

from mmpack_build.common import list_files, pushdir, popdir, \
    parse_soname, shlib_keyname


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


SonameData = namedtuple('SonameData',
                        ['soname', 'name', 'version', 'pkgname'])
REF_SONAME_DATA = [
    SonameData('libfoo.so.1', 'libfoo', '1', 'libfoo1'),
    SonameData('libfoo-1.dll', 'libfoo', '1', 'libfoo1'),
    SonameData('libfoo.so.1.2', 'libfoo', '1.2', 'libfoo1.2'),
    SonameData('libfoo-1.2.dll', 'libfoo', '1.2', 'libfoo1.2'),
    SonameData('libfoo1.2.so.3', 'libfoo1.2', '3', 'libfoo1.2-3'),
    SonameData('libfoo1.2-3.dll', 'libfoo1.2', '3', 'libfoo1.2-3'),
    SonameData('libfoo-bar.so.1', 'libfoo-bar', '1', 'libfoo-bar1'),
    SonameData('libfoo-bar-1.dll', 'libfoo-bar', '1', 'libfoo-bar1'),
    SonameData('libFoo-Bar.so.1', 'libFoo-Bar', '1', 'libfoo-bar1'),
    SonameData('libFoo-Bar-1.dll', 'libFoo-Bar', '1', 'libfoo-bar1'),
    SonameData('libfoo.so', 'libfoo', '', 'libfoo'),
    SonameData('libfoo.dll', 'libfoo', '', 'libfoo'),
    SonameData('libfoo1.2.so', 'libfoo1.2', '', 'libfoo1.2'),
    SonameData('libfoo1.2.dll', 'libfoo1.2', '', 'libfoo1.2'),
    SonameData('liba52-0.7.4.so', 'liba52-0.7.4', '', 'liba52-0.7.4'),
]

class TestCommon(unittest.TestCase):
    def setUp(self):
        pushdir('.')

    def tearDown(self):
        popdir()

    @classmethod
    def setUpClass(cls):
        makedirs(TEST_TREE_ROOT, exist_ok=True)
        pushdir(TEST_TREE_ROOT)

        for path in REF_FILELIST:
            # Create dummy file located at path
            dirpath = dirname(path)
            if dirpath:
                makedirs(dirpath, exist_ok=True)
            open(path, 'w')

        popdir()

    @classmethod
    def tearDownClass(cls):
        rmtree(TEST_TREE_ROOT)

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
        for refdata in REF_SONAME_DATA:
            name, version = parse_soname(refdata.soname)
            self.assertEqual(name, refdata.name)
            self.assertEqual(version, refdata.version)

    def test_shlib_name(self):
        """
        test shlib_keyname()
        """
        for refdata in REF_SONAME_DATA:
            pkgname = shlib_keyname(refdata.soname)
            self.assertEqual(pkgname, refdata.pkgname)
