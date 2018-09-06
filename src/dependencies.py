# @mindmaze_header@
'''
helper module containing functions to scan for dependencies within files
'''


from common import shell, remove_duplicates, filetype, get_host_arch
from elf_utils import elf_dpkg_deps, elf_deps


def mmpack_pkg_provides(filename):
    ''' Parse given file and return its package dependency list
    '''

    packagelist = []
    for lib in elf_deps(filename):
        cmd = 'mmpack provides ' + lib
        packagelist += shell(cmd).split(',')

    remove_duplicates(packagelist)
    return packagelist


def scan_dependencies(filename):
    'scan filename for dependency with the right file type'
    _, dist = get_host_arch()
    if dist != 'debian':
        raise NotImplementedError('unsupported distribution: ' + dist)

    ftype = filetype(filename)
    if ftype == 'elf':
        return elf_dpkg_deps(filename)

    # only elf format is supported for now
    return None
