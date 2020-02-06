#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation
"""
Server handling the upload of packages
"""

import os
import signal
import time
import sys
import glob
import shutil
import tarfile
from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

import parse_manifest as pm


# configuration global variable
OBSERVERS = []

DIR_UPLOAD = 'tmp'
DIR_PACKAGES = '.'

def clean_upload(tarname: str):
    """
    This function cleans the archive uploaded by the user and the residual files
    created to handle the upload.

    Args:
        tarname: name of the archive uploaded by the user
    """
    # suppress the residual files
    shutil.rmtree(DIR_UPLOAD)
    os.remove(tarname)


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

    def __init__(self, to_observe):
        super().__init__()
        self.to_observe = to_observe

    def on_created(self, event):
        """
        creation callback
        """
        tarname = os.path.basename(event.src_path).split('_')[0]
        print('Checking ', tarname)

        os.mkdir(DIR_UPLOAD)

        with tarfile.open(event.src_path, 'r:*') as tar:
            tar.extractall(DIR_UPLOAD)

            if not glob.glob(DIR_UPLOAD + '/*.mmpack-manifest'):
                clean_upload(event.src_path)

            for manifest in glob.glob(DIR_UPLOAD + '/*.mmpack-manifest'):
                if os.path.isfile(manifest):
                    if pm.try_upload(manifest, DIR_UPLOAD, DIR_PACKAGES +
                                     'binary-index', DIR_PACKAGES):
                        print('Proceeding data')
                        #mp.replace_files()
                    else:
                        print('Manifest or/and data are not coherent -> Abort')
                        clean_upload(event.src_path)


def main(to_observe):
    """
    entry point to update the mmpack db
    """
    # watch folder for changes
    observer = Observer()
    observer.schedule(MmpackDbObserver(to_observe),
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
    OBSERVERS.append(main(sys.argv[1]))

    # sleep until manual interrupt
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        cleanup_exit()
