# @mindmaze_header@

from importlib.util import spec_from_file_location, module_from_spec
from os.path import join
from sysconfig import get_paths


# Make this site module behave as a proxy to the standard site mode
_stdlib_site_py = join(get_paths()['stdlib'], 'site.py')
_spec = spec_from_file_location('_site', _stdlib_site_py)
_site = module_from_spec(_spec)
_spec.loader.exec_module(_site)


def __getattr__(name: str):
    return getattr(_site, name)


def __dir__():
    return dir(_site)

