from os.path import abspath, join
from sys import platform

_prefix_bindir = abspath(join(__file__, '../../../../bin'))

if platform == 'linux':
    from os import environ
    name = 'LD_LIBRARY_PATH'
    environ[name] = ':'.join(environ.get(name, [])
                             + [_prefix_bindir])
elif platform == 'win32':
    try:
        from os import add_dll_directory
        _MMPACK_DLL_DIR = add_dll_directory(_prefix_bindir)
    except ImportError:
        pass

