# @mindmaze_header@
"""
plugin tracking containing python file handling functions
"""

import filecmp
import os
import re
import shutil
from glob import glob
from typing import Set, Dict

from . base_hook import BaseHook, PackageInfo
from . common import shell
from . provide import Provide, ProvideList


# example of matches:
# 'lib/python3.6/site-packages/foo.so' => foo
# 'lib/python3/site-packages/_foo.so' => foo
# 'lib/python3/site-packages/foo_bar.py' => foo_bar
# 'lib/python3/site-packages/foo/__init__.py' => foo
# 'lib/python3/site-packages/foo/_internal.so' => foo
# 'lib/python2/site-packages/foo.so' => None
_PKG_REGEX = re.compile(r'lib/python3(?:\.\d)?/site-packages/_?([\w_]+)')


# location relative to the prefix dir where the files of public python packages
# must be installed
_MMPACK_REL_PY_SITEDIR = 'lib/python3/site-packages'


def _get_py3_public_import_name(filename: str) -> str:
    """
    Return the python3 package name a file should belongs to (ie the one with
    __init__.py file if folder, or the name of the single module if directly in
    site-packages folder).
    """
    res = _PKG_REGEX.search(filename)
    if not res:
        return None

    return res.groups()[0]


def _mmpack_pkg_from_pyimport_name(pyimport_name: str):
    """
    Return the name of the mmpack package that should provide the
    `pyimport_name` passed in argument.
    """
    return 'python3-' + pyimport_name.lower()


def _gen_pysymbols(pyimport_name: str, pkg: PackageInfo,
                   sitedir: str) -> Set[str]:
    script = os.path.join(os.path.dirname(__file__), 'python_provides.py')
    cmd = ['python3', script, '--site-path='+sitedir, pyimport_name]

    cmd_input = '\n'.join(pkg.files)
    cmd_output = shell(cmd, input_str=cmd_input)
    return set(cmd_output.split())


#####################################################################
# Python hook for mmpack-build
#####################################################################

class MMPackBuildHook(BaseHook):
    """
    Hook tracking python module used and exposed
    """

    def post_local_install(self):
        """
        install move python3 packages from versioned python3 install folder
        to a unversioned python3 folder. This way, python3 version can be
        upgraded (normally, only python3 standard library has to be
        installed in a version installed folder).
        """
        # Move all public package from unversioned python3 folder to an
        # unversioned one
        for pydir in glob('lib/python3.*/site-packages'):
            os.makedirs(_MMPACK_REL_PY_SITEDIR, exist_ok=True)
            for srcdir, dirs, files in os.walk(pydir):
                reldir = os.path.relpath(srcdir, pydir)
                dstdir = os.path.join(_MMPACK_REL_PY_SITEDIR, reldir)

                # Create the folders in unversioned python3 site package
                for dirpath in dirs:
                    os.makedirs(os.path.join(dstdir, dirpath), exist_ok=True)

                # Move files to unversioned python3 site package
                for filename in files:
                    src = os.path.join(srcdir, filename)
                    dst = os.path.join(dstdir, filename)

                    # If destination file exists, we have no issue if source
                    # and destination are the same
                    if os.path.lexists(dst):
                        if filecmp.cmp(src, dst):
                            continue
                        raise FileExistsError

                    os.replace(src, dst)

            # Remove the remainings
            shutil.rmtree(pydir)

    def get_dispatch(self, install_files: Set[str]) -> Dict[str, Set[str]]:
        pkgs = dict()
        for file in install_files:
            pyname = _get_py3_public_import_name(file)
            if pyname:
                mmpack_pkgname = _mmpack_pkg_from_pyimport_name(pyname)
                pkgfiles = pkgs.setdefault(mmpack_pkgname, set())
                pkgfiles.add(file)

        return pkgs

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        py3_provides = ProvideList('python')

        # Loop over all python modules contained in the mmpack package and
        # parse the python public entry point of it (ie the entry point of the
        # python package)
        for pyname in {_get_py3_public_import_name(f) for f in pkg.files}:
            # file not in python package will generate None, discard them
            if not pyname:
                continue

            root = '{}/{}'.format(_MMPACK_REL_PY_SITEDIR, pyname)
            if not {root+'/__init__.py', root+'.py'}.isdisjoint(pkg.files):
                symbols = _gen_pysymbols(pyname, pkg, _MMPACK_REL_PY_SITEDIR)
            else:
                raise RuntimeError('Not entry point found for python '
                                   'package {} in {}'
                                   .format(pyname, _MMPACK_REL_PY_SITEDIR))

            provide = Provide(pyname)
            provide.pkgdepends = _mmpack_pkg_from_pyimport_name(pyname)
            provide.add_symbols(symbols, self._version)
            py3_provides.add(provide)

        # update symbol information from .provides file if any
        py3_provides.update_from_specs(specs_provides, pkg.name)

        pkg.provides['python'] = py3_provides

    def store_provides(self, pkg: PackageInfo, folder: str):
        filename = '{}/{}.pyobjects'.format(folder, pkg.name)
        pkg.provides['python'].serialize(filename)
