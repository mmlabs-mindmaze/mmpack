# @mindmaze_header@
"""
python package that imports other packages
"""

from simple import main_dummy_fn, MainData as md
import multi


def _print_answer():
    a = 42
    print(a)
    return a


def argh(val: int) -> None:
    if _print_answer() != main_dummy_fn('hello', 2):
        return

    print('are same')


class FooBar(md):
    def __init__(self):
        super().__init__('somedata')
        self.new_data = truc()
        self.new_data.v1 = 66
        self.fullname = 'John Doe'

    def hello(self):
        print('hello')

_hello = FooBar()
_hello.disclose_private()
_truc = md('data')
_truc.disclose_private()
print(multi.THE_ANSWER)
