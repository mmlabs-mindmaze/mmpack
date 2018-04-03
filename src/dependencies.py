#!/usr/bin/env python3
'''
helper module containing functions to scan for dependencies within files
'''


from common import shell, remove_duplicates, filetype
from elf_utils import elf_pkg_deps, elf_deps


def mmpack_pkg_provides(filename):
    ''' Parse given file and return its package dependency list
    '''

    packagelist = []
    for lib in elf_deps(filename):
        cmd = 'mmpack provides ' + lib
        packagelist += shell(cmd).split(',')

    remove_duplicates(packagelist)
    return packagelist


def dependencies(filename):
    'scan filename for dependency with the right file type'
    ftype = filetype(filename)
    if ftype == 'elf':
        return elf_pkg_deps(filename)

    # only elf format is supported for now
    return None
