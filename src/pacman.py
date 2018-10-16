# @mindmaze_header@
'''
helper module containing pacman wrappers and file parsing functions
'''

from common import shell


def pacman_find_dependency(filename: str) -> str:
    '''find pacman package providing given file

    can take any file as parameter, but this is expected to get a soname

    Raises: ShellException if the package could not be found
    '''
    pacman_line = shell('pacman -Qo ' + filename)
    pacman_line = pacman_line.split('is owned by ')[-1]
    package, _version = pacman_line.split(' ')
    # It appears there can only be one package version and we cannot explict
    # a package version on install using the pacman command
    # ... rolling-release paragigm and all ...
    return package
