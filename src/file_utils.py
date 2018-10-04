# @mindmaze_header@
'''
Helpers to find out what files are
'''

import re
from os.path import islink


def is_manpage(filename: str) -> int:
    '''
    Returns: the manpage section on success
             -1 on error
    '''
    # eg matches:
    #  - man/file.1
    #  - /path/to/man/file.2
    #  - man2/file.3
    #  - man/man3/file.9
    match = re.match(r'.*man\d?/.*(\d)', filename)
    if match:
        return int(match.groups(0)[0])
    else:
        return -1


def is_exec_manpage(filename: str) -> bool:
    ' *.1 manpages'
    return is_manpage(filename) == 1


def is_devel_manpage(filename: str) -> bool:
    ' *.2, *.3 manpages'
    return is_manpage(filename) == 2 or is_manpage(filename) == 3


def is_doc_manpage(filename: str) -> bool:
    ' not *.1, *.2, *.3 manpages'
    return is_manpage(filename) > 3


def is_debugsym(filename: str) -> bool:
    'returns if a file is a debug symbols stripped from some binary'
    return filename.endswith('.debug')


def is_include(filename: str) -> bool:
    'returns if a file is a header (part of a devel package)'
    return 'include/' in filename


def is_documentation(filename: str) -> bool:
    'returns whether a file should be part of a documentation package'
    return 'doc/' in filename


def is_binary(filename: str) -> bool:
    'returns whether a file should be part of a binary package'
    return 'bin/' in filename


def is_libdevel(filename: str) -> bool:
    'returns whether a file is soname symlink'
    return filename.endswith('.so') and islink(filename)


def is_pkgconfig(filename: str) -> bool:
    'returns whether a file a pkgconfig file'
    return 'pkgconfig/' in filename


def is_devel(path: str) -> bool:
    'returns whether a file is development files'
    return (is_libdevel(path)
            or is_include(path)
            or is_devel_manpage(path)
            or is_pkgconfig(path))
