#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation

import os
import signal
import time
import sys
import glob
import shutil
from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

import parse_manifest as pm


# configuration global variable
OBSERVERS = []

def abort_upload(tarname: str):
    # suppress the residual files
    shutil.rmtree('tmp')
    os.remove(tarname)


class MmpackDbObserver(PatternMatchingEventHandler):
    """
    Observer to update mmpack database

    Listen for the creation of an archive tar.
    On creation of an archive, check the presence of a manifest in the archive,
    then check that the manifest and data received in the archive are coherent.
    If this is the case then proceed the update of the package database,
    otherwise abort and the residual files (the archive and the repository
    created to deal with the data).
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

        os.mkdir('tmp')

        with tarname.open(tarname, 'r:*') as tar:
            tar.extractAll('tmp')

            if not glob.glob('tmp/*.mmpack-manifest'):
                abort_upload(tarname)

            for manifest in glob.glob('tmp/*.mmpack-manifest'):
                if os.path.isfile(manifest):
                    if pm.check_manifest_and_data(manifest):
                        print('Proceeding data')
                        #mp.replace_files()
                    else:
                        print('Manifest or/and data are not coherent -> Abort')
                        abort_upload(tarname)


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
