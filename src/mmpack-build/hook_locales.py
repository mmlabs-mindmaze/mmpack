# @mindmaze_header@
"""
plugin to package internationalization files
"""

import re
from struct import iter_unpack, unpack
from typing import Dict, List, Set

from .base_hook import BaseHook
from .common import shell, Assert
from .file_utils import filetype
from .package_info import DispatchData, PackageInfo


def _extract_msgids_from_gnu_mo(filename: str) -> Set[str]:
    """
    Extract the original strings (msgid) of GNU MO file. The format is simple
    and described at:
    https://www.gnu.org/software/gettext/manual/gettext.html#MO-Files

    Returns:
        set of msgid if filename is indeed a GNU MO file, None otherwise
    """
    extension = filename.rsplit('.', maxsplit=1)[-1]
    if extension not in ('gmo', 'mo'):
        return None

    with open(filename, 'rb') as gmo_fp:
        # Read header and determine the endianness of the file from its magic
        # number
        magic = gmo_fp.read(4)
        if magic[:4] == b'\x95\x04\x12\xde':
            endianness = '>'
        elif magic[:4] == b'\xde\x12\x04\x95':
            endianness = '<'
        else:
            return None

        # Read whole file content in gmo_data
        gmo_fp.seek(0)
        gmo_data = gmo_fp.read()

    rev, num, toffset = unpack(endianness + 'III', gmo_data[4:16])
    if rev != 0:
        raise Assert(f'{filename} has an unsupported format revision {rev}')

    # Walk in all entries of original strings
    msgids = set()
    fmt = endianness + 'II'
    for slen, offset in iter_unpack(fmt, gmo_data[toffset:toffset+num*8]):
        msgid = gmo_data[offset:slen+offset].decode('utf-8')
        # '' is not an actual string entry (GNU MO metadata)
        if msgid:
            msgids.add(msgid)

    return msgids


def _get_execfmt_strings(filename: str) -> List[str]:
    """
    Get the strings of the executable format file
    """
    cmd = ['strings', '-d', '-w', '--output-separator=\x03', filename]
    return shell(cmd, log=False).split('\x03')


class MMPackBuildHook(BaseHook):
    """
    Hook tracking internationalization files
    """

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

        pkg = data.assign_to_pkg(self._srcname + '-locales', locales)
        if not pkg.description:
            pkg.description = self._src_description + \
                              '\nThis package the translation files.'

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        if pkg.ghost:
            return

        # Collect the translated strings from all message file contained in the
        # package
        msgids = set()
        for filename in pkg.files:
            mo_keys = _extract_msgids_from_gnu_mo(filename)
            if mo_keys:
                msgids.update(mo_keys)

        if msgids:
            pkg.provides['locales'] = msgids

    def update_depends(self, pkg: PackageInfo, other_pkgs: List[PackageInfo]):
        locales_pkgs = [p for p in other_pkgs
                        if p.name != pkg.name and 'locales' in p.provides]
        if not locales_pkgs or pkg.ghost:
            return

        # Collect all strings of data section in binary executable or shared
        # library. This should encompass translatated strings if any
        used_keys = set()
        for filename in pkg.files:
            file_type = filetype(filename)
            if file_type in ('pe', 'elf'):
                used_keys.update(_get_execfmt_strings(filename))

        # Inspect locales provided by other cobuilded package and establish a
        # dependency link if one of the string in data of current package match
        # one of the translated string.
        for other in locales_pkgs:
            if not used_keys.isdisjoint(other.provides['locales']):
                pkg.add_to_deplist(other.name)
