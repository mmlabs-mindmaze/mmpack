# @mindmaze_header@
"""
python package that imports other packages
"""

from multi import *
from multi.bar import Employee, Employee2
from simple import main_dummy_fn, MainData as main
from nspace.pkg_a import NSpacePkgAData as _PkgData


def _print_answer():
    a = 42
    print(a)

    e = Employee(name='Bla')
    print(e.name)
    e2 = Employee2(name2='boo')
    print(e2.name2)

    return a


def argh(val: int) -> None:
    if _print_answer() != main_dummy_fn('hello', 2):
        return

    print('are same')

    d = _PkgData(val, 64)
    d.show()


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
