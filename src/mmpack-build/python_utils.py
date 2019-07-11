# @mindmaze_header@
'''
helper module containing python file handling functions
'''
import re


# example of matches:
# 'lib/python3.6/site-packages/foo.so' => foo
# 'lib/python3/site-packages/_foo.so' => foo
# 'lib/python3/site-packages/foo_bar.py' => foo_bar
# 'lib/python3/site-packages/foo/__init__.py' => foo
# 'lib/python3/site-packages/foo/_internal.so' => foo
# 'lib/python2/site-packages/foo.so' => None
PKG_REGEX = re.compile(r'lib/python3(?:\.\d)?/site-packages/_?([\w_]+)')


def is_python3_pkgfile(filename: str) -> str:
    'Inspect if file belongs to a python package.'
    return PKG_REGEX.search(filename) is not None


def get_python3_pkgname(filename: str) -> str:
    '''Return the mmpack package a file should belong to.
       This function must be called after is_python_pkgfile() as been
       called on filename argument. Doing so guarantees that a meaningful
       package name will be returned and no exception will be raised.
    '''
    res = PKG_REGEX.search(filename)
    pypkg_name = res.groups()[0]
    return 'python3-' + pypkg_name.lower()
