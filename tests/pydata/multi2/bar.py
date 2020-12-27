# @mindmaze_header@
"""
python bar module
"""

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


A_BAR = None
