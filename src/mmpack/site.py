# @mindmaze_header@

from importlib.util import spec_from_file_location, module_from_spec
from os.path import abspath, join, exists
from sys import platform
from sysconfig import get_paths


# Make mmpack prefix folders usable searchable for libraries
if platform == 'linux':
    # Monkey ctypes in order to cdll.LoadLibrary search in prefix libdir
    from ctypes import _dlopen

    _libdir = abspath(join(__file__, '../../../../lib'))

    def _replace_dlopen(name, mode):
        path = join(_libdir, name)
        if '/' not in name and exists(path):
            return _dlopen(path, mode)
        return _dlopen(name, mode)

    import ctypes
    ctypes._dlopen = _replace_dlopen

elif platform == 'win32':
    try:
        from os import add_dll_directory
        _prefix_bindir = abspath(join(__file__, '../../../../bin'))
        _MMPACK_DLL_DIR = add_dll_directory(_prefix_bindir)
    except ImportError:
        pass


# Make this site module behave as a proxy to the standard site mode
_stdlib_site_py = join(get_paths()['stdlib'], 'site.py')
_spec = spec_from_file_location('_site', _stdlib_site_py)
_site = module_from_spec(_spec)
_spec.loader.exec_module(_site)


def __getattr__(name: str):
    return getattr(_site, name)


def __dir__():
    return dir(_site)

