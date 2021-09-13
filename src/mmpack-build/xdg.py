"""
implementation of XDG basedir environment variables specs
https://specifications.freedesktop.org/basedir-spec/basedir-spec-0.6.html

 * XDG_DATA_HOME : defines the base directory relative to which user specific
                   data files should be stored
 * XDG_CONFIG_HOME : defines the base directory relative to which user specific
                     configuration files should be stored.
 * XDG_CACHE_HOME : defines the base directory relative to which user specific
                    non-essential data files should be stored.
 * XDG_DATA_DIRS : defines the preference-ordered set of base directories to
                   search for data files in addition to the XDG_DATA_HOME
                   base directory.
 * XDG_CONFIG_DIRS : defines the preference-ordered set of base directories to
                     search for configuration files in addition to the
                     XDG_CONFIG_HOME base directory.
"""

from os import environ as env
from os.path import expanduser, join as path_join


# XDG default values
_HOME = env.get('HOME', expanduser('~'))
_XDG_DATA_HOME_DEFAULT = path_join(_HOME, '.local', 'share')
_XDG_DATA_DIRS_DEFAULT = '/usr/local/share:/usr/share'
_XDG_CONFIG_HOME_DEFAULT = path_join(_HOME, '.config')
_XDG_CONFIG_DIRS_DEFAULT = '/etc/xdg'
_XDG_CACHE_HOME_DEFAULT = path_join(_HOME, '.cache')


# XDG variables
XDG_DATA_HOME = env.get('XDG_DATA_HOME', _XDG_DATA_HOME_DEFAULT)
XDG_CONFIG_HOME = env.get('XDG_CONFIG_HOME', _XDG_CONFIG_HOME_DEFAULT)
XDG_CACHE_HOME = env.get('XDG_CACHE_HOME', _XDG_CACHE_HOME_DEFAULT)

# XDG path list variables
XDG_DATA_DIRS = ':'.join({XDG_DATA_HOME,  # noqa
        env.get('XDG_DATA_DIRS', _XDG_DATA_DIRS_DEFAULT)})
XDG_CONFIG_DIRS = ':'.join({XDG_CONFIG_HOME,  # noqa
        env.get('XDG_CONFIG_DIRS', _XDG_CONFIG_DIRS_DEFAULT)})


# explicit export of public variables
__all__ = [
    "XDG_DATA_HOME",
    "XDG_CONFIG_HOME",
    "XDG_CACHE_HOME",
    "XDG_DATA_DIRS",
    "XDG_CONFIG_DIRS",
]
