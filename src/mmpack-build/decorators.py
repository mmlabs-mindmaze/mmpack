# @mindmaze_header@
'''
simple usual decorators
'''


def run_once(function):
    '''
    Run decorated function only once.
    '''
    def _wrapper(*args, **kwargs):
        if not _wrapper.has_run:
            _wrapper.has_run = True
            return function(*args, **kwargs)
    _wrapper.has_run = False
    return _wrapper


class _SingletonWrapper(object):  # pylint: disable=too-few-public-methods
    '''
    A singleton wrapper class. Its instances would be created
    for each decorated class.
    '''

    def __init__(self, cls):
        self.__wrapped__ = cls
        self._instance = None

    def __call__(self, *args, **kwargs):
        '''Returns a single instance of decorated class'''
        if self._instance is None:
            self._instance = self.__wrapped__(*args, **kwargs)
        return self._instance


def singleton(cls):
    '''
    A singleton decorator. Returns a wrapper objects. A call on that object
    returns a single instance object of decorated class. Use the __wrapped__
    attribute to access decorated class directly in unit tests
    '''
    return _SingletonWrapper(cls)
