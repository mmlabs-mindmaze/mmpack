# @mindmaze_header@
"""
Helpers to find out what files are
"""

import re
from os.path import islink, basename, splitext

from common import shell


def filetype(filename):
    """
    get file type

    This performs the exact same operation as:
    file  --brief --preserve-date <filename>
    """
    if not islink(filename):
        # Open file and read magic number
        with open(filename, 'rb', buffering=0) as file:
            magic = file.read(4)

        # try to read file type first
        if magic[:4] == b'\x7fELF':
            return 'elf'
        if magic[:2] == b'MZ':
            return 'pe'

    # return file extension otherwise
    # eg. python, c-header, ...
    return splitext(filename)[1][1:].strip().lower()


def get_linked_dll(import_lib):
    """
    Get dll filename associated with import_lib
    """
    strlist = shell(['strings', import_lib], log=False).lower().split()
    dll_names = {v for v in strlist if v.endswith('.dll')}

    # Check we have one and only dll
    if len(dll_names) != 1:
        raise RuntimeError('Only one imported dll should be'
                           'found in {}. dll(s) found: {}'
                           .format(import_lib, dll_names))

    # Get the only element of dll_names set
    return dll_names.pop()


def is_dynamic_library(filename: str) -> str:
    """
    Return filetype if format conforms to a library

    eg. lib/libxxx.so.1.2.3
    - expects there is no leading './' in the pathname
    - is directly in the 'lib' directory
    - name starts with 'lib'
    - contains .so within name (maybe not at the end)
    """
    base = basename(filename)
    if (not islink(filename)
            and ((filename.startswith('lib/lib') and '.so' in base)
                 or (filename.startswith('bin/') and base.endswith('.dll')))):
        return filetype(filename)
    return None


def is_manpage(filename: str) -> int:
    """
    Returns:
        the manpage section on success
        -1 on error
    """
    # eg matches:
    #  - man/file.1
    #  - /path/to/man/file.2
    #  - man2/file.3
    #  - man/man3/file.9
    match = re.match(r'.*man\d?/.*(\d)', filename)
    if match:
        return int(match.groups(0)[0])
    return -1


def is_exec_manpage(filename: str) -> bool:
    """
    *.1 manpages
    """
    return is_manpage(filename) == 1


def is_devel_manpage(filename: str) -> bool:
    """
    *.2, *.3 manpages
    """
    return is_manpage(filename) == 2 or is_manpage(filename) == 3


def is_doc_manpage(filename: str) -> bool:
    """
    not *.1, *.2, *.3 manpages
    """
    return is_manpage(filename) > 3


def is_debugsym(filename: str) -> bool:
    """
    returns if a file is a debug symbols stripped from some binary
    """
    return filename.endswith('.debug')


def is_include(filename: str) -> bool:
    """
    returns if a file is a header (part of a devel package)
    """
    return 'include/' in filename


def is_documentation(filename: str) -> bool:
    """
    returns whether a file should be part of a documentation package
    """
    return 'doc/' in filename


def is_binary(filename: str) -> bool:
    """
    returns whether a file should be part of a binary package
    """
    return 'bin/' in filename


def is_libdevel(filename: str) -> bool:
    """
    returns whether a file is soname symlink
    """
    return filename.endswith('.so') and islink(filename)


def is_importlib(filename: str) -> bool:
    """
    returns whether a file is import library of a dll
    """
    return filename.endswith('.dll.a') or filename.endswith('.lib')


def is_pkgconfig(filename: str) -> bool:
    """
    returns whether a file a pkgconfig file
    """
    return 'pkgconfig/' in filename


def is_cmake_pkg_desc(filename: str) -> bool:
    """
    returns whether a file is a CMake package file descriptor
    """
    return filename.endswith('.cmake')


def is_devel(path: str) -> bool:
    """
    returns whether a file is development files
    """
    return (is_libdevel(path)
            or is_importlib(path)
            or is_include(path)
            or is_devel_manpage(path)
            or is_pkgconfig(path)
            or is_cmake_pkg_desc(path))
