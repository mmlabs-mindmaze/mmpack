#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation
"""
create, update, and prune package database

configuration file is a yaml file with two keys:
 - log-file: full-path to the file where the log will be written
 - repositories: list of mmpack repository paths to monitor
"""

import logging
import logging.handlers
import os
import signal
import sys
import time

from collections import *
from glob import glob
from typing import Union

import tarfile
import yaml

from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer


# configuration global variable
CONFIG_FILEPATH = '/etc/mmpack-db/config.yaml'
CONFIG = None
OBSERVERS = []

# logging
logger = None


def init_logger():
    """
    Init logger to print to *log_file*.

    log file is read from the config file 'log-file' key, and defaults
    to '/var/log/mmpack-db.log'

    Should be called after loading the CONFIG

    Files rotate every day, and are kept for a month
    """
    global logger  # pylint: disable=global-statement
    log_file = CONFIG.get('log-file', '/var/log/mmpack-db.log')
    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    logger = logging.getLogger('mmpack-db')
    logger.addHandler(log_handler)
    logger.setLevel(logging.INFO)


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    content = yaml.load(open(filename, 'rb').read(), Loader=yaml.BaseLoader)
    return content if content else {}


def yaml_serialize(obj: Union[list, dict], filename: str) -> None:
    """
    Save object as yaml file of given name
    """
    with open(filename, 'w+', newline='\n') as outfile:
        yaml.dump(obj, outfile,
                  default_flow_style=False,
                  allow_unicode=True,
                  indent=4)


def touch(filepath: str):
    """
    helper: create empty text file if it does not exist
    """
    if not os.path.exists(filepath):
        dirname = os.path.dirname(filepath)
        if not os.path.exists(dirname):
            os.mknod(dirname)
        with open(filepath, 'w'):
            pass


def parse_package(root, package):
    """
    extract and return the list of files contained by given package
    """
    # pylint: disable=invalid-name

    if os.path.isfile(os.path.join(root, package)):
        pkg_file = os.path.join(root, package)
    else:
        candidates = glob('{}/{}_*.mpk'.format(root, package))
        if not candidates:
            logging.error(' no candidate package found for {}'.format(package))
            return (None, None)
        pkg_file = glob('{}/{}_*.mpk'.format(root, package))[0]

    tar = tarfile.open(pkg_file, 'r:*')
    sha256sum_file = tar.extractfile('./var/lib/mmpack/metadata/'
                                     + package
                                     + '.sha256sums')

    files = dict()
    for f in sha256sum_file:
        files[f.decode('utf-8').split(':')[0].strip()] = package

    return files


def update_package(root, file_db, package):
    """
    update a single package
    """
    files = parse_package(root, package)

    logging.info('updating: {}'.format(package))
    file_db.update(files)


def delete_package(file_db, package):
    """
    delete a single package
    """
    logging.info('deleting: {}'.format(package))
    for key, value in file_db:
        if value == package:
            del file_db[key]


class MmpackDbObserver(PatternMatchingEventHandler):
    """
    Observer to update mmpack database

    Only listen for changes to mmpack packages within folder.
    On each change, update the state of both db and save
    """
    # only look at mmpack packages
    patterns = ['*.mpk']

    def __init__(self, root, file_db, file_db_path):
        super().__init__()
        self.root = root
        self.file_db = file_db
        self.file_db_path = file_db_path

    def on_created(self, event):
        """
        creation callback
        """
        pkg_name = os.path.basename(event.src_path).split('_')[0]
        update_package(self.root, self.file_db, pkg_name)
        yaml_serialize(self.file_db, self.file_db_path)

    def on_modified(self, event):
        """
        modification callback
        """
        pkg_name = os.path.basename(event.src_path).split('_')[0]
        delete_package(self.file_db, event.src_path)
        update_package(self.root, self.file_db, pkg_name)
        yaml_serialize(self.file_db, self.file_db_path)

    def on_deleted(self, event):
        """
        deletion callback
        """
        delete_package(self.file_db, event.src_path)
        yaml_serialize(self.file_db, self.file_db_path)


def main(root):
    """
    entry point to update the mmpack db
    """
    # check arguments
    if not os.path.isdir(root):
        raise ValueError('Invalid arguments: ' + root)

    # init vars
    file_db_path = os.path.join(root, 'mmpack-file-db.yaml')
    touch(file_db_path)
    file_db = yaml_load(file_db_path)

    # initial load of directory content
    index = yaml_load(os.path.join(root, 'binary-index'))
    for package in index:
        update_package(root, file_db, package)

    # first update of file-dbs
    yaml_serialize(file_db, file_db_path)

    # watch folder for changes
    observer = Observer()
    observer.schedule(MmpackDbObserver(root, file_db, file_db_path),
                      root, recursive=False)
    observer.start()
    return observer


def cleanup_exit(sig=None, frame=None):
    """
    clean observers and exit
    """
    # pylint: disable=unused-argument
    logging.info('exiting after signal {} ...'.format(sig))
    for observer in OBSERVERS:
        observer.stop()
    exit(0)


signal.signal(signal.SIGINT, cleanup_exit)


def load_config(filename):
    """
    load configuration file and return it as a dict
    """
    # pylint: disable=global-statement
    global CONFIG
    CONFIG = yaml.load(open(filename, 'rb', Loader=yaml.FullLoader).read())


if __name__ == '__main__':
    if len(sys.argv) == 2:
        load_config(sys.argv[1])
    else:
        load_config(CONFIG_FILEPATH)
    init_logger()

    for path in CONFIG.get('repositories'):
        OBSERVERS.append(main(path))

    # sleep until manual interrupt
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        cleanup_exit()
