# @mindmaze_header@
"""
Helpers to find out what files are
"""

import re
import sysconfig
from os.path import islink, basename, splitext

from .common import shell, wprint
from .errors import MMPackBuildError


# Match the interpreter of a shebang line (interpreter will be set in the first
# group). All the path component but the last one will be discarded unless it
# is env. The first argument will be returned then.
# Example of matches:
# '#!/bin/bash' -> 'bash'
# '#!/usr/bin/python3' -> 'python3'
# '#!/foo/bar' -> 'bar'
# '#!/usr/bin/env python3' -> 'python3'
# '#!/foo/bar python3' -> 'bar'
_SHEBANG_REGEX = re.compile(br'#!\s*(?:/[^ \n/]+)*/(?:env\s+)?([^\s]+)')


# pylint: disable=too-many-return-statements
def filetype(filename):
    """
    get file type

    This performs the similar operation as:
    file  --brief --preserve-date <filename>

    If the file has a shebang, the interpreter name will be returned
    """
    if not islink(filename):
        try:
            # Open file and read magic number (binary)
            with open(filename, 'rb', buffering=0) as fileobj:
                magic = fileobj.read(8)
        except IsADirectoryError:
            return 'directory'

        # try to read file type first
        if magic[:4] == b'\x7fELF':
            return 'elf'
        if magic[:2] == b'MZ':
            return 'pe'
        if magic[:7] == b'!<arch>':
            return 'ar'
        if magic[:4] == b'\x50\x4b\x03\x04':
            return 'zip'
        if magic[:2] == b'#!':
            # the file contains a shebang. So the complete first line of file
            # and parse the interpreter. Search for end of line is performed in
            # binary to avoid the UTF-8 decoder encounter invalid UTF-8 char
            # beyond shebang line (can happen since read is buffered)
            with open(filename, 'rb') as stream:
                shebang_line = stream.readline()
            shebang_match = _SHEBANG_REGEX.match(shebang_line)
            if shebang_match:
                return shebang_match.groups()[0].decode()

    # return file extension otherwise
    # eg. python, c-header, ...
    return splitext(filename)[1][1:].strip().lower()


def get_linked_dll(import_lib):
    """
    Get dll name associated with import_lib

    Args:
        import_lib: path to import library file

    Return:
        The linked DLL name if one is found, None if the import library imports
        actually no library.

    Raises:
        RuntimeError: more than one dll name could be found in the import lib
    """
    strlist = shell(['strings', import_lib], log=False).lower().split()
    dll_names = {v for v in strlist if v.endswith('.dll')}

    # Check the import lib is not dangling (no matching dll). The case of
    # dangling import library, although rare, may happen in the case of bad
    # build system (example of automake/libtool generating dll.a for module).
    # Hence only log a warning in this case
    if not dll_names:
        wprint(f'No imported DLL could be found in {import_lib}')
        return None

    # Check we have no more than one dll
    if len(dll_names) > 1:
        raise MMPackBuildError('Only one imported dll should be found in '
                               f'{import_lib}. dlls found: {dll_names}')

    # Get the only element of dll_names set
    return dll_names.pop()


def get_exec_fileformat(host_archdist: str) -> str:
    """
    Get executable file format compatible with host ('pe' on windows,
    elf' on linux...)

    Args:
        host_archdist: full host specification (like 'amd64-debian')

    Returns:
        the executable file format string ('pe' or 'elf')
    """
    if host_archdist.endswith('-windows'):
        return 'pe'

    return 'elf'


def is_dynamic_library(filename: str, host_archdist: str) -> bool:
    """
    Test whether a filename is actually shared lib compatible with the host
    architecture/distribution.

    eg. lib/libxxx.so.1.2.3
    - expects there is no leading './' in the pathname
    - is directly in the 'lib' directory
    - name starts with 'lib'
    - contains .so within name (maybe not at the end)

    Args:
        filename: path of file to test
        host_archdist: full host specification (like 'amd64-debian')

    Returns:
        True if filename is a dynamic library

    Raises:
        NotImplementedError: unhandled executable file format
    """
    base = basename(filename)
    fmt = get_exec_fileformat(host_archdist)

    # Check that filename and path match the installation folder and naming
    # scheme of a public shared library of the host executable file format
    if fmt == 'elf':
        multiarch = sysconfig.get_config_var('MULTIARCH')
        multiarch = multiarch if multiarch else ''
        # Example of matches:
        # 'lib/libfoo.so'
        # 'lib/libfoo.so.1'
        # 'lib/libfoo.so.1.2.3'
        # 'usr/lib/libfoo.so.1.2.3'
        # 'usr/lib/libfoo++.so.1.2.3'
        # 'usr/lib/x86_64-linux-gnu/libfoo.so.1.2.3'
        # 'lib/libfoo_awesome-special.so.1.2.3'
        # 'lib/libfoo45.so.1.2.3'
        # 'lib/libfoo3.5.so.1.2.3'
        elffile = rf'(?:usr/)?lib/(?:{multiarch}/)?lib(?:[+.\w-]+)\.so[.0-9]*'
        if not re.match(elffile, filename):
            return False
    elif fmt == 'pe':
        if not ('bin/' in filename and base.endswith('.dll')):
            return False
    else:
        raise NotImplementedError(f'Unhandled exec format: {fmt}')

    if islink(filename):
        return False

    return filetype(filename) == fmt


def is_manpage(filename: str) -> int:
    """
    Returns:
        the manpage section on success
        -1 on error
    """
    # eg matches:
    #  - share/man/man1/file.1
    #  - share/man/man2/file.2
    match = re.fullmatch(r'share/man/man\d?/.*\.(\d)', filename)
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
    return 'share/doc/' in filename or 'share/doc-base/' in filename


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


def is_staticlib(filename: str) -> bool:
    """
    returns whether a file is import library of a dll
    """
    return filetype(filename) == 'ar' and not filename.endswith('.dll.a')


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
            or is_staticlib(path)
            or is_importlib(path)
            or is_include(path)
            or is_devel_manpage(path)
            or is_pkgconfig(path)
            or is_cmake_pkg_desc(path))


def is_python_script(filename: str) -> bool:
    """
    returns whether a file is a python script
    """
    return filetype(filename) in ('py', 'python', 'python3')
