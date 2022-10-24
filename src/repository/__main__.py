# [@]mmpack_header[@]
"""
Tool handling the upload of packages to a repository
"""
# pylint: disable=wrong-import-position
# pylint: disable=invalid-name

import signal
import sys
import time
import os
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from io import TextIOBase
from typing import Tuple, Union

import yaml

from watchdog.events import PatternMatchingEventHandler
from watchdog.observers import Observer

try:
    from argcomplete import autocomplete
except ImportError:
    # pylint: disable=missing-function-docstring,unused-argument
    def autocomplete(parser: ArgumentParser, **kwargs):
        pass

from .repo import Repo  # noqa


# configuration global variable
OBSERVERS = []


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

    def __init__(self, to_observe, repo: Repo):
        super().__init__()
        self.to_observe = to_observe
        self.repo = repo

    def on_created(self, event):
        """
        creation callback
        """
        self.repo.try_handle_upload(event.src_path)


def cleanup_exit(sig=None, frame=None):
    """
    clean observers and exit
    """
    # pylint: disable=unused-argument
    for observer in OBSERVERS:
        observer.stop()
    sys.exit(0)


def watch_folder_for_change(repo: Repo, to_observe: str):
    """
    entry point to update the repository
    """
    # watch folder for changes
    observer = Observer()
    observer.schedule(MmpackDbObserver(to_observe, repo),
                      to_observe, recursive=False)
    observer.start()
    OBSERVERS.append(observer)
    signal.signal(signal.SIGINT, cleanup_exit)

    # sleep until manual interrupt
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        cleanup_exit()


def batch_cmd_add(repo: Repo, manifest: str) -> Tuple[bool, str]:
    """
    Add packages listed in manifest to repository
    """
    if not manifest:
        return (False, 'missing argument')

    success = repo.stage_upload(manifest)
    if not success:
        return (False, 'adding manifest failed')

    return (success, '')


def batch_cmd_remove_src(repo: Repo, argument: str) -> Tuple[bool, str]:
    """
    Remove source and associated binary package
    """
    valid_args = {'name', 'version', 'srcsha'}

    if not argument:
        return (False, 'missing argument')

    args = {n[0].strip(): n[1].strip()
            for n in arg.split('=')
            for arg in argument.split(',')}

    if not set(args).issubset(valid_args):
        return (False, 'Invalid options: {set(args).difference(valid_args)}')

    success = repo.stage_remove_matching_src(**args)
    if not success:
        return (False, 'remove source failed')

    return (success, '')


def batch_updates(repo: Repo, initial_change: bool):
    """
    Read stdin and perform repository update specified in each line
    This is useful to implement a repository update in a subprocess
    communicating over pipes.
    """
    if initial_change:
        repo.begin_changes()

    for line in sys.stdin:
        cmdargs = line.strip().split(maxsplit=1)
        if not cmdargs:
            continue

        if len(cmdargs) == 1:
            cmdargs.append('')

        cmd = cmdargs[0]
        args = cmdargs[1]

        success = True
        if cmd == 'ADD':
            success, errmsg = batch_cmd_add(repo, args)
        elif cmd == 'REMOVE_MATCHING_SRC':
            success, errmsg = batch_cmd_remove_src(repo, args)
        elif cmd == 'COMMIT':
            repo.commit_changes()
            repo.begin_changes()
        elif cmd == 'ROLLBACK':
            repo.rollback_changes()
            repo.begin_changes()
        elif cmd == 'BEGIN_CHANGES':
            repo.begin_changes()
        elif cmd == 'COMMIT_CHANGES':
            repo.commit_changes()
        elif cmd == 'ROLLBACK_CHANGES':
            repo.rollback_changes()
        else:
            success = False
            errmsg = 'unknown command'

        print('OK' if success else ('FAIL ' + errmsg), flush=True)

    repo.rollback_changes()


def _remove_src(repo: Repo, cmd_opts):
    repo.begin_changes()
    success = repo.stage_remove_matching_src(name=cmd_opts.name,
                                             version=cmd_opts.version,
                                             srcsha=cmd_opts.srcsha)
    if success:
        repo.commit_changes()
    else:
        repo.rollback_changes()

    print('Removal succes' if success else 'removal failed')
    return success


def load_repository(opts) -> Repo:
    """
    Load repository according to command line options
    """
    repo_path = opts.repo_path
    os.makedirs(repo_path, exist_ok=True)

    # Load metadata from repo or create if not existing
    metadata_path = os.path.join(repo_path, 'metadata')
    try:
        metadata = yaml.load(open(metadata_path, 'rt'), Loader=yaml.BaseLoader)
    except FileNotFoundError:
        # Create initial repo metadata if new repo
        if not opts.repo_arch:
            print('New repository must have architecture specified',
                  file=sys.stderr)
            sys.exit(1)
        metadata = {'architecture': opts.repo_arch}
        yaml.dump(metadata, open(metadata_path, 'wt'),
                  default_flow_style=False)

    repo_arch = metadata['architecture']
    return Repo(repo_path, repo_arch)


_TRUE_STRINGS = {'true', 't', 'yes', 'y', 'on', '1'}
_FALSE_STRINGS = {'false', 'f', 'no', 'n', 'off', '0'}


def str2bool(value: str) -> bool:
    """
    Convert a string to bool value

    Raise:
        TypeError: value is not a string
        ValueError: the string value does not represent a boolean
    """
    lowerval = value.strip().lower()
    if lowerval in _TRUE_STRINGS:
        return True
    if lowerval in _FALSE_STRINGS:
        return False

    raise ValueError('"{}" does not represent a boolean value'.format(value))


class MixedIO:
    """File-like accepting both bytes and str"""

    def __init__(self, stream: TextIOBase):
        self._file = stream

    def write(self, data: Union[bytes, str]) -> int:
        """Same as IOBase.write"""
        if isinstance(data, bytes):
            data = data.decode()
        return self._file.write(data)

    def flush(self):
        """Same as IOBase.flush"""
        self._file.flush()


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)

    parser.add_argument('-p', '--path', default='.',
                        action='store', dest='repo_path', type=str,
                        help='path to repository')
    parser.add_argument('-a', '--arch',
                        action='store', dest='repo_arch', type=str,
                        help='architecture of repository if created')

    subparsers = parser.add_subparsers(dest='cmd', required=True)

    # watch command parser
    watch_subparser = subparsers.add_parser('watch')
    watch_subparser.add_argument('to_observe',
                                 help='folder to watch for incoming manifest')

    # update command parser
    update_subparser = subparsers.add_parser('add')
    update_subparser.add_argument('manifest', help='manifest file')

    # update command parser
    update_subparser = subparsers.add_parser('remove-src')
    update_subparser.add_argument('--name', help='source package name')
    update_subparser.add_argument('--version', help='source package version')
    update_subparser.add_argument('--srcsha', help='source package sha256')

    # batch command parser
    batch_subparser = subparsers.add_parser('batch')
    batch_subparser.add_argument('--initial-change', dest='initial_change',
                                 nargs='?', default=True, const=True,
                                 action='store', type=str2bool,
                                 help='begin changes after command startup')

    autocomplete(parser, output_stream=MixedIO(sys.stdout))
    return parser.parse_args()


def main() -> int:
    """
    main program entry
    """
    opts = parse_options()
    repo = load_repository(opts)
    success = True

    if opts.cmd == 'watch':
        watch_folder_for_change(repo, opts.to_observe)
    elif opts.cmd == 'add':
        success = repo.try_handle_upload(opts.manifest, remove_upload=False)
        print('manifest added' if success else 'update failed')
    elif opts.cmd == 'remove-src':
        success = _remove_src(repo, opts)
    elif opts.cmd == 'batch':
        batch_updates(repo, opts.initial_change)

    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
