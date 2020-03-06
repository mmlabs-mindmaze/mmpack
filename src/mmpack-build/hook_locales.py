# @mindmaze_header@
"""
plugin to package internationalization files

TODO: analyze the strings used in other cobuilded packages to generate the
right relation between the -locales package and those that use the translated
strings
"""

import re
from gettext import GNUTranslations
from typing import Dict, List

from . base_hook import BaseHook
from . common import shell
from . file_utils import filetype
from . package_info import DispatchData, PackageInfo


def _is_gnu_mo(filename: str):
    extension = filename.rsplit('.', maxsplit=1)
    if extension not in ('gmo', 'mo'):
        return False

    magic = open(filename, 'rb', buffering=False).read(4)
    return magic[:4] == b'\x95\x04\x12\xde' or magic[:4] == b'\xde\x12\x04\x95'


def _get_data_strings(filename: str):
    cmd = ['strings', '-d', '--output-separator=\x03', filename]
    return shell(cmd, log=False).split('\x03')


class MMPackBuildHook(BaseHook):
    """
    Hook tracking internationalization files
    """
    _has_locales = False

    def dispatch(self, data: DispatchData):
        """
        Unless specified otherwise in mmpack specs, all internationalization
        files are packaged together in a dedicated package.

        The rationale for this policy is that:
          1 - translations are (usually) not manadatory for program to function
          2 - translations are often shared for all libraries and executable
              provided by the same project.
          3 - if locales are used exclusivily by a specific lib and executable,
              nothing guarantees that this will be the case in future.
          4 - if a shared lib package contains its translation files, this will
              prevent smooth transition in case of ABI break of a the lib: the
              abi break will result in two shared lib packages containing the
              same files, which will prevent to coinstall those two packages
              (which is the guarantee for a smooth transition).
        """
        locales_re = re.compile(r'(usr/|mingw64/)?share/locale/.*')
        locales = {f for f in data.unassigned_files if locales_re.match(f)}
        if not locales:
            return

        self._has_locales = True
        pkg = data.assign_to_pkg(self._srcname + '-locales', locales)
        if not pkg.description:
            pkg.description = self._src_description + \
                              '\nThis package the translation files.'

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        if not self._has_locales or pkg.ghost:
            return

        # Collect the translated strings from all message file contained in the
        # package
        msgids = set()
        for filename in pkg.files:
            if not _is_gnu_mo(filename):
                continue

            mo_keys = GNUTranslations(open(filename, 'rb'))._catalog.keys()
            msgids.update(mo_keys)

        msgids.discard('')  # workaround: the catalog has special '' entry
        pkg.provides['locales'] = msgids

    def update_depends(self, pkg: PackageInfo, other_pkgs: List[PackageInfo]):
        if not self._has_locales or pkg.ghost:
            return

        # Collect all strings of data section in binary executable or shared
        # library. This should encompass translatated strings if any
        used_keys = set()
        for filename in pkg.files:
            file_type = filetype(filename)
            if file_type not in ('pe', 'elf'):
                continue
            used_keys.update(_get_data_strings(filename))

        # Inspect locales provided by other cobuilded package
        for other in other_pkgs:
            locale_msgids = pkg.provides.get('locales')
            if other == pkg or not locale_msgids:
                continue

            # Compute the number of strings that are present in binary that
            # appear to be also strings that are translated. If this number is
            # 33% of the number of translated strings, we consider that the
            # package uses the locale in the other package.
            used_msgids = locale_msgids.intersection(used_keys)
            if len(used_msgids) > (len(locale_msgids) / 3):
                pkg.add_to_deplist(other.name)
