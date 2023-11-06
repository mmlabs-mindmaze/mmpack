# @mindmaze_header@
"""
The mmpack_build python package

mmpack_build is a collection of many independent internal modules.
The following are currently presented:
 * Do not present mmpack commands nor hooked plugins

 * Do not present platform-specific submodules (elf, pe, dpkg ...)
   Those must be explicitly called:
   >>> import mmpack_build.dpkg

 * Present everything else so that internals are called
   like below:
   >>> import mmpack_build
   >>> v = mmpack_build.mm_version.Version('1.0.0')

"""
# pylint: disable=invalid-name

import sys
from os.path import dirname, join as join_path

sys.path.insert(0, join_path(dirname(__file__), '_vendor'))

from . import base_hook
from . import binary_package
from . import common
from . import file_utils
from . import mm_version
from . import provide
from . import src_package
from . import source_tarball
from . import workspace
from . import xdg
