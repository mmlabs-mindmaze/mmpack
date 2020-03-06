# @mindmaze_header@
"""
plugin to package internationalization files

TODO: analyze the strings used in other cobuilded packages to generate the
right relation between the -locales package and those that use the translated
strings
"""

import re
from gettext import GNUTranslations

from . base_hook import BaseHook
from . package_info import DispatchData


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
        locales = {f for f in data.unassigned_files if locales_re.match(f)}
        if not locales:
            return

        pkg = data.assign_to_pkg(self._srcname + '-locales', locales)
        if not pkg.description:
            pkg.description = self._src_description + \
                              '\nThis package the translation files.'

    def update_provides(self, pkg: PackageInfo,
                        specs_provides: Dict[str, Dict]):
        mo_files = [f for f in 
        msgid = {}

        for filename in pkg.files:
            if not filename.enswith('.mo'):
                continue

            # Try opening the file as GNU mo file. In case of OSError failure,
            # this is the wrong file type. Then just ignore
            try:
                translation = GNUTranslations(open(filename, 'rb'))
            except OSError:
                continue

            msgid            

        pkg.provides['locales'] = {}
