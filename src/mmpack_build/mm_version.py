# @mindmaze_header@
"""
version manipulation utility
"""
from __future__ import annotations

import re
from typing import Optional

import yaml

from .common import mm_representer


class Version:
    """
    Simple version class

    * recognizes digits so that: "1.2" == "1.02", and "1.2" < "1.10"
    * Use string comparison otherwise: "1x" < "1y"
    * adds "any" as version wildcard
    """
    _COMP_RE = re.compile(r'(\d+)')

    def __init__(self, string: Optional[str] = None):
        if not string:
            string = 'any'
        elif '_' in string:
            raise ValueError(f'invalid version {string}')

        self._string = string
        self._comps = self._COMP_RE.split(string)
        for ind, value in enumerate(self._comps):
            try:
                self._comps[ind] = int(value)
            except ValueError:
                pass

    def is_any(self):
        """Return True is version wildcard"""
        return self._string == 'any'

    def __lt__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps < other._comps

    def __le__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps <= other._comps

    def __eq__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps == other._comps

    def __ne__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps != other._comps

    def __gt__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps > other._comps

    def __ge__(self, other):
        if self.is_any() or other.is_any():
            return True
        return self._comps >= other._comps

    def __str__(self):
        return self._string

    def __repr__(self):
        return str(self)


yaml.add_representer(Version, mm_representer)
