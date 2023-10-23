# @mindmaze_header@
"""
Representation of loaded sources-strap file
"""

import re
from os.path import abspath, basename, exists, join as join_path
from typing import Any, Dict, List

import yaml
from .common import get_name_version_from_srcdir


def _multiple_replace(lookup: Dict[str, str], text: str) -> str:
    # Create a regular expression from all of the dictionary keys
    regex = re.compile('|'.join(lookup.keys()))

    # For each match, look up the corresponding value in the dictionary
    return regex.sub(lambda match: lookup[match.group(0)], text)


class SourceStrapSpecs:
    """source-strap config"""

    def __init__(self, srcdir: str):
        name, version = get_name_version_from_srcdir(srcdir)
        lookup = {
            '@MMPACK_NAME@': name,
            '@MMPACK_VERSION@': version,
        }
        try:
            path = join_path(srcdir, 'mmpack/sources-strap')
            with open(path, encoding='utf-8') as stream:
                content = _multiple_replace(lookup, stream.read())
                specs = yaml.load(content, Loader=yaml.BaseLoader)
        except FileNotFoundError:
            specs = {}

        self.depends: List[str] = specs.pop('depends', [])
        self.upstream_method: Optional[str] = specs.pop('method', None)
        self.upstream_url: Optional[str] = specs.pop('url', None)
        self.patches: List[str] = specs.pop('patches', [])
        self._opts: Dict[str, str] = specs

        self._validate()

    def get(self, *args) -> Any:
        """Get optional value"""
        return self._opts.get(*args)

    def _validate(self):
        if self.upstream_method:
            if self.upstream_method not in ('git', 'tar'):
                raise Assert('Invalid method ' + self.upstream_method)

            if not self.upstream_url:
                raise Assert('upstream method specified but url missing')


