# @mindmaze_header@
"""
python foo module
"""

from multi import utils

THE_ANSWER = 42


class DummyData:
    def __init__(self, val: int):
        self.an_attr = 'Hello'
        self.a_number = val
        self.v1 = 0

    def useless_method(self):
        pass


def main_dummy_fn(an_arg: str, num: int):
    """
    Main entry function
    """
    ans = THE_ANSWER
    he_say_str = '{} says {}'.format(an_arg, num)
    i_say_str = 'I say {}'.format(ans)

    return [an_arg, he_say_str, 'me', i_say_str]


def somefunc(val: int) -> DummyData:
    _print_answer_multi()
    return DummyData(val)


def _init_exported_list():
    return [1, 2, 3, 4, 5]


class MainData:
    """
    Main dummy data
    """

    a_class_attr = 'someattr'

    def __init__(self, data = str):
        self.data1 = data
        self._non_public_data = 'this is private'
        self.fullname = 'Ema Nymton'

    def _what_does_he_says(self) -> str:
        return _non_public_data

    def disclose_private(self, who: str) -> str:
        return '{} says: {}'.format(who, self._what_does_he_says())

A_CLASS = MainData('hello')
A_CLASS.fullname = 'john'
EXPORTED_LIST = _init_exported_list()

