#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation
"""
Server handling the upload of packages

Usage:

python3 server.py *name_directory_to_listen*
"""

import signal
import time
import sys
from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

import Repo


# configuration global variable
OBSERVERS = []

DIR_DB = 'packages-database'


class MmpackDbObserver(PatternMatchingEventHandler):
    """
    Observer to update mmpack database

    Listen for the creation of an archive tar (uploaded by a user).
    On creation of an archive, check the presence of a manifest in the archive,
    then check that the manifest and data received in the archive are coherent.
    If this is the case then proceed the update of the package database,
    otherwise abort the upload (meaning suppress the residual files: the archive
    uploaded by the user and the directory created to deal with the data).
    """
    # only look at mmpack packages
    patterns = ['*.tar']

    def __init__(self, to_observe, architecture):
        super().__init__()
        self.to_observe = to_observe
        self.architecture = architecture

    def on_created(self, event):
        """
        creation callback
        """
        repo = Repo(DIR_DB, self.architecture)
        repo.upload(event.src_path)
        repo.deinit()

def main(to_observe: str, architecture: str):
    """
    entry point to update the mmpack db
    """
    # watch folder for changes
    observer = Observer()
    observer.schedule(MmpackDbObserver(to_observe, architecture),
                      to_observe, recursive=False)
    observer.start()
    return observer


def cleanup_exit(sig=None, frame=None):
    """
    clean observers and exit
    """
    # pylint: disable=unused-argument
    for observer in OBSERVERS:
        observer.stop()
    exit(0)


signal.signal(signal.SIGINT, cleanup_exit)


if __name__ == '__main__':
    OBSERVERS.append(main(sys.argv[1], sys.argv[2]))

    # sleep until manual interrupt
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        cleanup_exit()
