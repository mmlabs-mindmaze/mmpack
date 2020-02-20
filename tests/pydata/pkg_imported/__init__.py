# @mindmaze_header@
"""
python package that imports other packages
"""

from multi import *
from simple import main_dummy_fn, MainData as main


def _print_answer():
    a = 42
    print(a)
    return a


def argh(val: int) -> None:
    if _print_answer() != main_dummy_fn('hello', 2):
        return

    print('are same')


class FooBar(main):
    def __init__(self):
        super().__init__('somedata')
        self.new_data = somefunc(24)
        self.new_data.v1 = 66
        self.fullname = 'John Doe'

    def hello(self):
        print('hello')

_hello = FooBar()
_hello.disclose_private('No one')
