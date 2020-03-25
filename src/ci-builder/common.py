# @mindmaze_header@
"""
common usage facilities
"""

from typing import Iterable

import logging


LOGGER = None


def init_logger(log_file):
    """
    Init logger to print to *log_file*.

    log file is read from the config file 'log-file' key, and defaults
    to '/var/log/gerrit-mmpack-build.log'

    Should be called after loading the CONFIG

    Files rotate every day, and are kept for a month
    """
    global LOGGER  # pylint: disable=global-statement

    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    LOGGER = logging.getLogger('gerrit-mmpack-build')
    LOGGER.addHandler(log_handler)
    LOGGER.setLevel(logging.INFO)


def log_error(msg: str):
    """
    Log message with error level
    """
    LOGGER.error(msg)


def log_info(msg: str):
    """
    Log message with info level
    """
    LOGGER.info(msg)


def subdict(adict: dict, keys: Iterable) -> dict:
    """
    Return a subset of adict dictionary containing only keys passed in argument
    """
    return {k: v for k, v in adict.items() if k in keys}
