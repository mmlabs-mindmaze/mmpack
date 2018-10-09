# @mindmaze_header@
'''
helper module containing pe parsing functions
'''

import os
from typing import Set

import pefile


def soname(filename: str) -> str:
    'Return the SONAME of given library'
    return os.path.basename(filename)


def soname_deps(filename) -> Set(str):
    'Parse given pe file and return its dependency soname list'
    pe_file = pefile.PE(filename)

    soname_set = set()
    try:
        for dll in pe_file.DIRECTORY_ENTRY_IMPORT:
            soname_set.add(soname(dll.dll.decode('utf-8')))
    except AttributeError:  # parsed pe file has no IMPORT section
        pass

    return soname_set


def symbols_list(filename, default_version):
    ''' Parse given pe file and return its exported symbols as a dictionary.
        dict keys are symbols name, values will be the symbols version
    '''
    pe_file = pefile.PE(filename)

    export_dict = {}
    try:
        for sym in pe_file.DIRECTORY_ENTRY_EXPORT.symbols:
            export_dict.update({sym.name.decode('utf-8'): default_version})
    except AttributeError:  # parsed pe file has no EXPORT section
        return {}

    return export_dict
