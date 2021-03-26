# @mindmaze_header@
"""
python bar module
"""

from typing import NamedTuple


def print_hello():
    print('hello')


class Bar:
    def __init__(self):
        self._full = True

    def drink(self):
        self._full = False


class _NeverEndingBar(Bar):
    def __init__(self):
        super(self).__init__()

    def drink(self):
        self._full = True


Employee = NamedTuple('Employee', [('name', str), ('id', int)])

class Employee2(NamedTuple):
    name2: str
    id2: int


A_BAR = None
