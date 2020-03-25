# @mindmaze_header@
"""
common usage facilities
"""

# logging
logger = None


def init_logger(log_file):
    """
    Init logger to print to *log_file*.

    log file is read from the config file 'log-file' key, and defaults
    to '/var/log/gerrit-mmpack-build.log'

    Should be called after loading the CONFIG

    Files rotate every day, and are kept for a month
    """
    global logger  # pylint: disable=global-statement

    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    logger = logging.getLogger('gerrit-mmpack-build')
    logger.addHandler(log_handler)
    logger.setLevel(logging.INFO)


def log_error(msg: str):
    """
    Log message with error level
    """
    log_error(msg)


def log_info(msg: str):
    """
    Log message with info level
    """
    log_info(msg)
