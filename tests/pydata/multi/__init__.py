# @mindmaze_header@
"""
python package that is made of several modules with __init__.py
"""

from . foo import somefunc, MainData, THE_ANSWER as _THE_ANSWER
from . import bar


def _print_answer_multi():
    print(_THE_ANSWER)



class FooBar(MainData):
    def __init__(self):
        super().__init__('somedata')
        self.new_data = somefunc(42)
        self.new_data.v1 = 66
        self.fullname = 'John Doe'

    def hello(self):
        print('hello')
