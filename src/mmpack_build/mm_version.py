# @mindmaze_header@
"""
version manipulation utility based on python's distutil LooseVersion class

LooseVersion is described as:
  Version numbering for anarchists and software realists.

  A version number consists of a series of numbers, separated by either periods
  or strings of letters. When comparing version numbers, the numeric components
  will be compared numerically, and the alphabetic components lexically.

There are no invalid version number.
Still LooseVersion expects *at least* one digit within the version string.
"""

from distutils.version import LooseVersion

import yaml

from .common import mm_representer


class Version(LooseVersion):  # pylint: disable=too-few-public-methods
    """
    Simple version class

    * inherited from LooseVersion:
      - recognizes digits so that: "1.2" == "1.02", and "1.2" < "1.10"
      - Use string comparison otherwise: "1x" < "1y"
    * adds "any" as version wildcard
    """

    def __init__(self, string):
        if not string:
            string = 'any'
        elif '_' in string:
            raise ValueError(f'invalid version {string}')

        super().__init__(string)

    def is_any(self):
        """
        LooseVersion asserts its string description contains at least one
        digit. We need to explicitly request its string description to
        prevent raising a TypeError
        """
        return str(self) == "any"

    def __lt__(self, other):
        if self.is_any() or other.is_any():
            return True
        try:
            return super().__lt__(other)
        except TypeError:
            return str(self) < str(other)

    def __le__(self, other):
        if self.is_any() or other.is_any():
            return True
        try:
            return super().__le__(other)
        except TypeError:
            return str(self) < str(other)

    def __eq__(self, other):
        if self.is_any() or other.is_any():
            return True
        return str(self.version) == str(other.version)

    def __ne__(self, other):
        if self.is_any() or other.is_any():
            return True
        return not self.__eq__(other)

    def __gt__(self, other):
        if self.is_any() or other.is_any():
            return True
        return not self.__le__(other)

    def __ge__(self, other):
        if self.is_any() or other.is_any():
            return True
        return not self.__lt__(other)

    def __repr__(self):
        return str(self)


yaml.add_representer(Version, mm_representer)
