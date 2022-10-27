# @mindmaze_header@
"""
A custom dumper for pyyaml in order to output format nicely specs and info
files
"""

from typing import Dict, List, Any

from yaml import Dumper, MappingNode, ScalarNode


_SPECS_ORDER = [
    'name',
    'source',
    'version',
    'ghost',
    'srcsha256',
    'sumsha256sums',
    'maintainer',
    'url',
    'licenses',
    'copyright',
    'build-system',
    'build-options',
    'syspkg-srcnames',
    'build-depends',
    'depends',
    'sysdepends',
    'files',
    'ignore',
    'description',
]


def _sorted_items(mapping: Dict[str, Any],
                  known_keys: List[str]) -> List[str]:
    """
    sort list following a specific order for head and tail of the list
    """
    labels = list(mapping.keys())

    head = [e for e in known_keys if e in labels]
    remaining_labels = set(labels)
    remaining_labels.difference_update(set(known_keys))
    sorted_labels = head + sorted(remaining_labels)

    return [(k, mapping[k]) for k in sorted_labels]


# pylint: disable=too-many-ancestors
class MMPackDumper(Dumper):
    """
    Override of usual dumper to suit format of mmpack files
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.level = 0
        self.key = None

    def represent_scalar(self, tag, value, style=None):
        if self.key == 'description' and '\n' in value:
            style = '|'
        return super().represent_scalar(tag, value, style)

    def represent_sequence(self, tag, sequence, flow_style=None):
        if self.key == 'licenses':
            flow_style = True
        return super().represent_sequence(tag, sequence, flow_style)

    # pylint: disable=too-many-branches
    def represent_mapping(self, tag, mapping, flow_style=None):
        """
        This implementation is mostly the one from yaml.BaseRepresenter. It has
        been tweaked to fit the format of mmpack/specs or MMPACK/info files and
        to keep track whick key of mapping is being rendered and which nested
        level.
        """
        value = []
        node = MappingNode(tag, value, flow_style)
        if self.alias_key is not None:
            self.represented_objects[self.alias_key] = node
        best_style = True
        if hasattr(mapping, 'items'):
            try:
                if self.level <= 2:
                    # format content of general and custom packages
                    mapping_items = _sorted_items(mapping, _SPECS_ORDER)
                else:
                    # Usual formatting
                    mapping_items = sorted(mapping.items())
            except TypeError:
                mapping_items = list(mapping.items())
        else:
            mapping_items = mapping

        prev_key = self.key
        for item_key, item_value in mapping_items:
            node_key = self.represent_data(item_key)

            # Format the value of mapping
            self.level += 1
            self.key = item_key
            node_value = self.represent_data(item_value)
            self.level -= 1

            # block style is selected if we have nested collection node
            if not (isinstance(node_key, ScalarNode)
                    and node_key.style):
                best_style = False
            if not (isinstance(node_value, ScalarNode)
                    and not node_value.style):
                best_style = False

            value.append((node_key, node_value))
        self.key = prev_key

        if flow_style is None:
            if self.default_flow_style is not None:
                node.flow_style = self.default_flow_style
            else:
                node.flow_style = best_style
        return node
