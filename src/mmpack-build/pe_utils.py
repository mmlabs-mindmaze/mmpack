# @mindmaze_header@
"""
helper module containing pe parsing functions
"""

import os
from glob import glob
from os.path import join as joinpath
from typing import Optional

import pefile

from .common import shell
from .decorators import singleton
from .workspace import Workspace

# pylint: disable=unused-import
# PE module does not need to change anything from the base Provide class
from .provide import Provide as ShlibProvide


def soname(filename: str) -> str:
    """
    Return the SONAME of given library
    """
    return os.path.basename(filename)


@singleton
class SystemLibs(set):
    """
    cache of system libraries
    """

    def __init__(self):
        # pylint: disable=super-init-not-called

        winroot = os.getenv('SYSTEMROOT')
        sysdirs = (
            winroot,
            joinpath(winroot, 'system32'),
        )

        for path in sysdirs:
            self.update({f.lower() for f in os.listdir(path)
                         if f.endswith(('.dll', '.DLL'))})


def build_id(filename: str) -> Optional[str]:
    """
    return build id note. Returns None if not found.
    """
    pe_file = pefile.PE(filename)
    try:
        for dbgsec in pe_file.DIRECTORY_ENTRY_DEBUG:
            try:
                entry = dbgsec.entry
                return b''.join((entry.Signature_Data1.to_bytes(4, 'big'),
                                 entry.Signature_Data2.to_bytes(2, 'big'),
                                 entry.Signature_Data3.to_bytes(2, 'big'),
                                 entry.Signature_Data4)).hex()
            except AttributeError:
                continue
    except AttributeError:
        pass
    finally:
        pe_file.close()

    return None


def soname_deps(filename):
    """
    Parse given pe file and return its dependency soname list
    """
    pe_file = pefile.PE(filename)

    soname_set = set()
    try:
        for dll in pe_file.DIRECTORY_ENTRY_IMPORT:
            dll = dll.dll.decode('utf-8').lower()
            if dll not in SystemLibs():
                soname_set.add(dll)
    except AttributeError:  # parsed pe file has no IMPORT section
        pass

    return soname_set


def symbols_set(filename):
    """
    Parse given pe file and return its exported symbols as a set.
    """
    pe_file = pefile.PE(filename)

    export_set = set()
    try:
        for sym in pe_file.DIRECTORY_ENTRY_EXPORT.symbols:
            export_set.add(sym.name.decode('utf-8'))
    except AttributeError:  # parsed pe file has no EXPORT section
        return set()

    return export_set


def undefined_symbols(filename):
    """
    Parse given pe file and return its undefined symbols set
    """
    # pefile populates its objects while parsing the file
    # (which is a bad idea)
    # as a result, pe_file does not have all its attributes
    # defined by default
    # pylint: disable=no-member
    pe_file = pefile.PE(filename)

    undefined_symbols_set = set()
    for dll in pe_file.DIRECTORY_ENTRY_IMPORT:
        if dll.dll.decode('utf-8').lower() in SystemLibs():
            continue

        for sym in dll.imports:
            undefined_symbols_set.add(sym.name.decode('utf-8'))

    return undefined_symbols_set


def get_dll_from_soname(library_name: str) -> str:
    """
    get dll from soname
    """
    # FIXME: workaround
    path = shell('which ' + library_name).strip()
    return shell('cygpath -m ' + path).strip()
