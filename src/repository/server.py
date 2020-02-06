#!/usr/bin/env python3
"""
Server handling the upload of packages

Usage:

python3 server.py *name_directory_to_listen*
"""

import signal
import sys
import time
from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

import repo


# configuration global variable
OBSERVERS = []

REPO_DIR = 'packages-database'


class MmpackDbObserver(PatternMatchingEventHandler):
    """
    Observer to update repository

    Listen for the upload of a mmpack-manifest file.
    On apparition of such a file, check that the manifest and data received are
    coherent. If this is the case then proceed the update of the repository,
    otherwise abort the upload (meaning suppress the residual files: the files
    uploaded by the user and the directory created to deal with the data).
    """
    patterns = ['*.mmpack-manifest']

    def __init__(self, to_observe, architecture):
        super().__init__()
        self.to_observe = to_observe
        self.architecture = architecture
        self.repo = repo.Repo(to_observe, REPO_DIR, architecture)

    def on_created(self, event):
        """
        creation callback
        """
        self.repo.try_handle_upload(event.src_path)


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
