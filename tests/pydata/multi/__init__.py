# @mindmaze_header@
"""
python package that is made of several modules with __init__.py
"""

from . foo import main_dummy_fn, MainData
from . import bar


def _print_answer():
    print(_THE_ANSWER)


def argh(val: int) -> None:
    _print_answer()


class FooBar(MainData):
    def __init__(self):
        super().__init__('somedata')
        self.new_data = truc()
        self.new_data.v1 = 66
        self.fullname = 'John Doe'

    def hello(self):
        print('hello')
