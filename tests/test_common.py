# @mindmaze_header@
import unittest

from os import environ, makedirs, getcwd, chdir
from os.path import dirname, abspath
from shutil import rmtree

from mmpack_build.common import list_files, parse_soname, shlib_keyname, \
    str2bool, wrap_str


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

TEST_TREE = environ.get('TESTSDIR', '.') + '/test_tree'
TEST_TREE_ROOT = TEST_TREE + '/root'


REF_SONAME_DATA = [
    ('libfoo.so.1', 'libfoo', '1', 'libfoo1'),
    ('libfoo-1.dll', 'libfoo', '1', 'libfoo1'),
    ('libfoo.so.1.2', 'libfoo', '1.2', 'libfoo1.2'),
    ('libfoo-1.2.dll', 'libfoo', '1.2', 'libfoo1.2'),
    ('libfoo1.2.so.3', 'libfoo1.2', '3', 'libfoo1.2-3'),
    ('libfoo1.2-3.dll', 'libfoo1.2', '3', 'libfoo1.2-3'),
    ('libfoo-bar.so.1', 'libfoo-bar', '1', 'libfoo-bar1'),
    ('libfoo-bar-1.dll', 'libfoo-bar', '1', 'libfoo-bar1'),
    ('libFoo-Bar.so.1', 'libFoo-Bar', '1', 'libfoo-bar1'),
    ('libFoo-Bar-1.dll', 'libFoo-Bar', '1', 'libfoo-bar1'),
    ('libfoo.so', 'libfoo', '', 'libfoo'),
    ('libfoo.dll', 'libfoo', '', 'libfoo'),
    ('libfoo1.2.so', 'libfoo1.2', '', 'libfoo1.2'),
    ('libfoo1.2.dll', 'libfoo1.2', '', 'libfoo1.2'),
    ('foo-1.dll', 'foo', '1', 'libfoo1'),
    ('foo.dll', 'foo', '', 'libfoo'),
    ('liba52-0.7.4.so', 'liba52-0.7.4', '', 'liba52-0.7.4'),
]


_LIPSUM = \
'''Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus.
Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, ultricies sed,
dolor. Cras elementum ultrices diam. Maecenas ligula massa, varius a, semper
congue, euismod non, mi. Proin porttitor, orci nec nonummy molestie, enim est
eleifend mi, non fermentum diam nisl sit amet erat. Duis semper. Duis arcu
massa, scelerisque vitae, consequat in, pretium a, enim. Pellentesque congue.
Ut in risus volutpat libero pharetra tempor. Cras vestibulum bibendum augue.
Praesent egestas leo in pede. Praesent blandit odio eu enim. Pellentesque sed
dui ut augue blandit sodales. Vestibulum ante ipsum primis in faucibus orci
luctus et ultrices posuere cubilia Curae; Aliquam nibh. Mauris ac mauris sed
pede pellentesque fermentum. Maecenas adipiscing ante non diam sodales
hendrerit.'''.replace('\n', ' ')


_LIPSUM_REF_70_3INDENT = \
'''Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non 
   risus. Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, 
   ultricies sed, dolor. Cras elementum ultrices diam. Maecenas ligula 
   massa, varius a, semper congue, euismod non, mi. Proin porttitor, 
   orci nec nonummy molestie, enim est eleifend mi, non fermentum diam 
   nisl sit amet erat. Duis semper. Duis arcu massa, scelerisque vitae, 
   consequat in, pretium a, enim. Pellentesque congue. Ut in risus 
   volutpat libero pharetra tempor. Cras vestibulum bibendum augue. 
   Praesent egestas leo in pede. Praesent blandit odio eu enim. 
   Pellentesque sed dui ut augue blandit sodales. Vestibulum ante ipsum 
   primis in faucibus orci luctus et ultrices posuere cubilia Curae; 
   Aliquam nibh. Mauris ac mauris sed pede pellentesque fermentum. 
   Maecenas adipiscing ante non diam sodales hendrerit.'''


_LIPSUM_REF_40_PIPEINDENT = \
'''Lorem ipsum dolor sit amet, consectetur 
|adipiscing elit. Sed non risus. 
|Suspendisse lectus tortor, dignissim 
|sit amet, adipiscing nec, ultricies 
|sed, dolor. Cras elementum ultrices 
|diam. Maecenas ligula massa, varius a, 
|semper congue, euismod non, mi. Proin 
|porttitor, orci nec nonummy molestie, 
|enim est eleifend mi, non fermentum 
|diam nisl sit amet erat. Duis semper. 
|Duis arcu massa, scelerisque vitae, 
|consequat in, pretium a, enim. 
|Pellentesque congue. Ut in risus 
|volutpat libero pharetra tempor. Cras 
|vestibulum bibendum augue. Praesent 
|egestas leo in pede. Praesent blandit 
|odio eu enim. Pellentesque sed dui ut 
|augue blandit sodales. Vestibulum ante 
|ipsum primis in faucibus orci luctus et 
|ultrices posuere cubilia Curae; Aliquam 
|nibh. Mauris ac mauris sed pede 
|pellentesque fermentum. Maecenas 
|adipiscing ante non diam sodales 
|hendrerit.'''

_DEPLIST_STR = \
'''dummy (>= 1.2.3-5), foo (= 5.2.3), bar (>= 77.99.3), helloworld,
fakepkg | not-a-real-package (>= 23), baz'''.replace('\n', ' ')

_WRAPPED_DEPLIST_REF45 = \
'''dummy (>= 1.2.3-5), foo (= 5.2.3), 
bar (>= 77.99.3), helloworld, 
fakepkg | not-a-real-package (>= 23), baz'''


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
        rmtree(TEST_TREE)

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
        chdir(TEST_TREE)
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

    def test_str2bool(self):
        """
        test str2bool()
        """
        refdata_for_true = ['true', 'TRUE', 'True',
                            'on', 'ON', 'On',
                            ' on', 'on ', ' on ', 'on \n', '\n on',
                            'yes', 'YES', 'Yes',
                            '1']
        refdata_for_false = ['false', 'FALSE', 'False',
                             'off', 'OFF', 'Off',
                            ' off', 'off ', ' off ', 'off \n', '\n off',
                             'no', 'NO', 'No',
                             '0']
        refdata_exception = ['dummy', 'tru',  'truefalse', 'true, false', '2']

        for value in refdata_for_true:
            self.assertIs(str2bool(value), True)

        for value in refdata_for_false:
            self.assertIs(str2bool(value), False)

        for value in refdata_exception:
            self.assertRaises(ValueError, str2bool, value)

    def test_wrap_str(self):
        """
        test wrap_str()
        """
        small_str = 'Hello world!'
        strtest = wrap_str(small_str, maxlen=70)
        self.assertEqual(strtest, small_str)

        strtest = wrap_str(_LIPSUM, maxlen=70, indent='   ')
        self.assertEqual(strtest, _LIPSUM_REF_70_3INDENT)

        strtest = wrap_str(_LIPSUM, maxlen=40, indent='|')
        self.assertEqual(strtest, _LIPSUM_REF_40_PIPEINDENT)

        strtest = wrap_str(_DEPLIST_STR, maxlen=45, split_token=', ')
        self.assertEqual(strtest, _WRAPPED_DEPLIST_REF45)
