try:
    from os import add_dll_directory
    from os.path import abspath, join

    _DLL_DIR = add_dll_directory(abspath(join(__file__, '../../../../bin')))
except ImportError:
    pass
