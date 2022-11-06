# @mindmaze_header@
# pylint: disable=logging-format-interpolation
"""
Module provinding helpers.
- It provides a helper to check that all the information required in a manifest
are present.
- It provides a helper to check that the data provided by a user are coherent
with the information given in the manifest.
- It provides a helper to replace the files upload with the old files (if any)
"""

from __future__ import annotations

from collections import Counter, namedtuple
from email.parser import Parser
from enum import Enum, auto
from hashlib import sha256
from subprocess import PIPE, Popen
from sysconfig import get_platform
import gzip
import logging
import logging.handlers
import os
import re
import shutil
from typing import Any, Dict, List, Optional, Callable, Set, TextIO
import yaml


class _LockingMethod(Enum):
    NONE = auto()
    FCNTL = auto()
    MSVCRT = auto()


# Import the right module for file locking
_USE_LOCKING_METHOD = _LockingMethod.NONE
try:
    import fcntl
    _USE_LOCKING_METHOD = _LockingMethod.FCNTL
except ImportError:
    pass
try:
    import msvcrt
    _USE_LOCKING_METHOD = _LockingMethod.MSVCRT
except ImportError:
    pass

# Use bsdtar on msys2 because it is not linked with msys2-0.dll... This avoids
# to mess the arguments
_TARPROG = 'bsdtar' if get_platform().startswith('mingw') else 'tar'


RELPATH_BINARY_INDEX = 'binary-index.gz'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'working_dir'
LOG_FILE = 'mmpack-repo.log'


IndicesStates = namedtuple('IndicesStates',
                           ['binindex', 'srcindex', 'counter'])


# The functions sha256sum, yaml_serialize, and yaml_load
# are functions that are extracted from mmpack-build/common.py. There are
# copied in this file in order to avoid the import of the module common and
# therefore in order to avoid to have a dependence.

CONFIG = {'debug': True, 'verbose': True}
TMP_LOG_STRLIST = []


def sha256sum(filename: str) -> str:
    """
    compute the SHA-256 hash of a file

    Args:
        filename: path of file whose hash must be computed

    Returns:
        a string containing hexadecimal value of hash
    """
    sha = sha256()
    with open(filename, 'rb') as stream:
        sha.update(stream.read())
    hexdig = sha.hexdigest()

    return hexdig


def _escape_newline(text: str) -> str:
    escaped = ''

    pos = 0
    for occurrence in re.finditer(r'\n+', text):
        start, end = occurrence.span(0)
        escaped += text[pos:start]
        escaped += text[start:end].replace('\n', '\n.')
        if end != len(text):
            escaped += '\n'

        pos = end

    escaped += text[pos:]
    return escaped


def wrap_str(text: str,
             maxlen: int = 80,
             indent: str = '',
             split_token: str = ' ') -> List[str]:
    """
    Wrap a text string

    Args:
        text: the string to wrap
        maxlen: the maximum line allowed before wrapping
        indent: string to insert before each new line after split
        split_token: the token after each a line may be split

    Return:
        the wrapped text
    """
    lines = []

    while len(text) > maxlen:
        prefix = text[:maxlen].rsplit(split_token, 1)[0]
        prefix += split_token
        text = text[len(prefix):]
        lines.append(prefix)

    lines.append(text)

    return ('\n' + indent).join(lines)


def _write_keyval(stream: TextIO, key: str, value: Any,
                  split: Optional[str] = None):
    if not isinstance(value, str):
        if isinstance(value, bool):
            value = str(value).lower()
        else:
            value = str(value)

    # Skip field with empty value
    if not value:
        return

    text = f'{key}: {value}'
    if split is None:
        stream.write(text + '\n')
        return

    first = True
    for line in text.split('\n'):
        wrapped_line = wrap_str(line if first else ' ' + line,
                                indent=' ', split_token=split)
        stream.write(wrapped_line + '\n')
        first = False


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    with open(filename, 'rb') as stream:
        return yaml.load(stream.read(), Loader=yaml.BaseLoader)


def _init_logger(log_file: str) -> logging.Logger:
    """
    Init logger to print to *log_file*.
    Files rotate every day, and are kept for a month

    Args:
        log_file: name of the file where the log are stored.
    """
    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    logger = logging.getLogger(os.path.abspath(log_file))
    logger.addHandler(log_handler)
    logger.setLevel(logging.INFO)

    return logger


class _BinPkg:
    def __init__(self, name: str = '', data: Optional[Dict[str, Any]] = None):
        self.name = name
        self._data = {}

        if not data:
            return

        # Copy with unfolding value of each field
        for key, val in data.items():
            self._data[key] = val.replace('\n .\n ', '\n').replace('\n ', '')

    def __getattr__(self, key: str) -> Any:
        try:
            return self._data[key]
        except KeyError as error:
            raise AttributeError(f'unknown attribute {key}') from error

    def update(self, data: Dict[str, Any]):
        """
        Update fields from key/val of data
        """
        self._data.update(data)

    def write_keyvals(self, stream: TextIO):
        """
        Write list of key/values part for the binary package at hand.
        """
        _write_keyval(stream, 'name', self.name)

        for key in ('version', 'source', 'srcsha256',
                    'ghost', 'sumsha256sums', 'depends', 'sysdepends'):
            _write_keyval(stream, key, self._data.get(key, ''))

        multiline_desc = _escape_newline(self.description)
        _write_keyval(stream, 'description', multiline_desc, split=' ')

        # Write at the end the repo specific fields
        for key in ('filename', 'size', 'sha256'):
            _write_keyval(stream, key, self._data[key])

        stream.write('\n')

    def srcid(self) -> str:
        """
        Get a unique identifier of the associated source
        """
        return _srcid(self.source, self.srcsha256)

    @staticmethod
    def load(pkg_path: str) -> _BinPkg:
        """
        This function reads a mpk file and returns a _BinPkg describing it

        Args:
            pkg_path: the path through the mpk file to read.
        """
        parser = Parser()

        cmd = [_TARPROG, '-xOf', pkg_path, './MMPACK/metadata']
        with Popen(cmd, stdout=PIPE, text=True, encoding='utf-8') as proc:
            metadata = parser.parse(proc.stdout)

        cmd = [_TARPROG, '-xOf', pkg_path, metadata['pkginfo-path']]
        with Popen(cmd, stdout=PIPE, text=True, encoding='utf-8') as proc:
            pkginfo = parser.parse(proc.stdout)

        data = dict(pkginfo)
        data['sumsha256sums'] = metadata['sumsha256sums']
        return _BinPkg(metadata['name'], data)


def file_serialize(index: dict, filename: str):
    """
    This function serializes a dictionary into a flat structure in a file.

    Args:
        index: dictionary to serialize.
        filename: file in which to serialize the dictionary.
    """
    with open(filename, 'w', newline='\n') as outfile:
        for value in index.values():
            lines = ['{}: {}\n'.format(k, v) for k, v in value.items()]
            outfile.write(''.join(lines) + '\n')


def _srcid(name: str, srcsha256: str) -> str:
    return name + '_' + srcsha256


def file_load(filename: str) -> dict:
    """
    This functions loads the contain of a file under the shape of a dictionary.

    Args:
        filename: file to read and to transform into a dictionary.
    """
    srcindex = {}
    entry = {}

    with open(filename, 'r') as stream:
        for line in stream:
            if not line.strip():
                src_id = _srcid(entry['name'], entry['sha256'])
                srcindex[src_id] = entry.copy()
                continue

            key, value = line.split(': ')
            entry[key] = value.rstrip('\n')

    return srcindex


def _create_empty_if_not_exist(filename: str):
    """
    Ensures filename exists, create it as empty if needed
    """
    if not os.path.isfile(filename):
        with open(filename, 'w+'):
            pass


class Repo:
    """
    This class model a repository.
    """
    # pylint: disable=too-few-public-methods
    # pylint: disable=too-many-instance-attributes

    def __init__(self, repo: str, architecture: str):
        """
        Initializes a repository: retrieves the path of the working directory
        and the directory containing all the packages (named repo_dir),
        retrieves the data under the form of dictionary of the repository
        binary-index and of the repository source-index, creates a logger,
        retrieves the architecture on which the packages of the repository can
        be deployed, and registers the number of binary packages referencing
        each source package.

        Args:
            repo: the directory where the packages, the binary-index and
                       the source-index are stored.
            architecture: architecture on which the packages of the repository
                          can be deployed.
        """
        abs_path_repo = os.path.abspath(repo)
        if not os.path.isdir(abs_path_repo):
            os.mkdir(abs_path_repo)

        self.binindex_file = os.path.join(abs_path_repo, RELPATH_BINARY_INDEX)
        self.srcindex_file = os.path.join(abs_path_repo, RELPATH_SOURCE_INDEX)
        _create_empty_if_not_exist(self.binindex_file)
        _create_empty_if_not_exist(self.srcindex_file)

        self.repo_dir = abs_path_repo
        self.working_dir = os.path.abspath(os.path.join(abs_path_repo,
                                                        RELPATH_WORKING_DIR))
        self.logger = _init_logger(os.path.join(self.repo_dir, LOG_FILE))

        self.arch = architecture
        self.binindex = {}
        self.srcindex = {}
        self.count_src_refs = Counter()
        self._reload_indices_from_files()

        self.last_update = 0
        self.lockfile = None

        self.to_remove = set()
        self.to_add = set()
        self.backup = IndicesStates(srcindex=self.srcindex,
                                    binindex=self.binindex,
                                    counter=self.count_src_refs)
        shutil.rmtree(self.working_dir, ignore_errors=True)

    def _reload_indices_from_files(self):
        self.last_update = os.stat(self.binindex_file).st_mtime

        self.binindex = {}
        parser = Parser()
        with gzip.open(self.binindex_file, 'rt') as index_fp:
            # iterate over package definition (section separated by empty line)
            for pkgdesc in re.split('\n\n+', index_fp.read()):
                pkgdata = dict(parser.parsestr(pkgdesc))
                if not pkgdata:
                    continue

                name = pkgdata.pop('name')
                self.binindex[name] = _BinPkg(name, pkgdata)

        self.srcindex = file_load(self.srcindex_file)
        if self.srcindex is None:
            self.srcindex = {}

        # count_src_refs: dictionary where the keys correspond to hash of
        # source packages and the values correspond to the number of binary
        # packages referencing the key.
        self.count_src_refs = Counter()
        for binpkg in self.binindex.values():
            self.count_src_refs[binpkg.srcid()] += 1

    def _check_hash(self, package: dict):
        """
        Check that the hash provided in a dictionary representing a package is
        true: there exists effectively a file with the name of the package with
        this sha256.

        Note that to really test the coherence between the manifest and a
        package, we normally have to test that the information in the
        dictionary provided by the keys 'file', 'size' and 'sha256' are true:
        there exists a file respecting all these properties. However we can
        only test the sha256 and everything else is done freely:
        - the field 'file' will be checked while calling the function sha256sum
        which will raise an exception in case the path of the package is not a
        file.
        - We assume that if the sha256 is correct then the size should also be
        correct.

        Args:
            package: dictionary representing a package.
        """
        filename = package['file']
        filepath = os.path.join(self.working_dir, filename)

        if sha256sum(filepath) != package['sha256']:
            raise ValueError

    def _lock_repo(self):
        # pylint: disable=consider-using-with
        self.lockfile = open(os.path.join(self.repo_dir, 'lock'), 'w')
        if _USE_LOCKING_METHOD == _LockingMethod.FCNTL:
            fcntl.flock(self.lockfile.fileno(), fcntl.LOCK_EX)
        elif _USE_LOCKING_METHOD == _LockingMethod.MSVCRT:
            msvcrt.locking(self.lockfile.fileno(), msvcrt.LK_LOCK, 4096)

    def _unlock_repo(self):
        # closing file will also release the lock
        self.lockfile.close()

    def _clear_change_data(self):
        self.to_add.clear()
        self.to_remove.clear()
        self.backup = IndicesStates(srcindex=self.srcindex,
                                    binindex=self.binindex,
                                    counter=self.count_src_refs)
        shutil.rmtree(self.working_dir)
        self._unlock_repo()

    def begin_changes(self):
        """
        Initiates a new set of stagged changes. The states of source and binary
        indices and counter are stored internally to be reverted later if
        necessary during a call to rollback_changes()
        """
        self._lock_repo()

        # Reload indices if attribute data is outdated
        if self.last_update < os.stat(self.binindex_file).st_mtime:
            self._reload_indices_from_files()

        self.backup = IndicesStates(srcindex=self.srcindex.copy(),
                                    binindex=self.binindex.copy(),
                                    counter=self.count_src_refs.copy())
        os.mkdir(self.working_dir)

    def rollback_changes(self):
        """
        Cancel the changes done in the source and binary indices and counters
        since last call to begin_changes().
        """
        self.srcindex = self.backup.srcindex
        self.binindex = self.backup.binindex
        self.count_src_refs = self.backup.counter
        self._clear_change_data()

    def commit_changes(self):
        """
        Moves the packages asked to be uploaded from the working directory to
        the repository. Removes the packages of the repository that are not
        needed anymore.
        """
        for filename in self.to_remove:
            self.logger.info('Remove {} from repository'.format(filename))
            file_path = os.path.join(self.repo_dir, filename)
            os.remove(file_path)

        for filename in self.to_add:
            self.logger.info('Add {} in repository'.format(filename))
            source = os.path.join(self.working_dir, filename)
            destination = os.path.join(self.repo_dir, filename)
            os.replace(source, destination)

        self.last_update = os.stat(self.binindex_file).st_mtime
        self._clear_change_data()

    def _write_keyvals_binindex(self, dirpath: str = None):
        filename = os.path.join(dirpath, RELPATH_BINARY_INDEX)

        with gzip.open(filename, 'wt', newline='\n') as stream:
            for pkg in self.binindex.values():
                pkg.write_keyvals(stream)

    def _dump_indexes_working_dir(self, to_add: set):
        """
        Writes the updated binary-index and source-index into the working
        directory. If no problem occurs while writing these files, then this
        function updates the dictionaries of the repository (referencing the
        data of the binary-index and of the source-index) and moves the
        binary-index and source-index previously written into the repository.

        Args:
            to_add: set to fill with packages that MUST be removed once we are
                    sure that the upload will be a success.
        """
        new_srcfile = os.path.join(self.working_dir, RELPATH_SOURCE_INDEX)
        file_serialize(self.srcindex, new_srcfile)

        self._write_keyvals_binindex(self.working_dir)

        to_add.add(RELPATH_SOURCE_INDEX)
        to_add.add(RELPATH_BINARY_INDEX)

    def _matching_srcids(self, srcname: str = None, version: str = None,
                         srcsha256: str = None) -> Set[str]:
        """
        Get the set of source package id that match the passed criteria
        """
        srcids = set()

        for srcid, srcinfo in self.srcindex.items():
            if srcname and srcinfo['name'] != srcname:
                continue

            if version and srcinfo['version'] != version:
                continue

            if srcsha256 and srcinfo['sha256'] != srcsha256:
                continue

            srcids.add(srcid)

        return srcids

    def _remove_srcpkg(self, src_id: str, to_remove: str):
        srcinfo = self.srcindex.pop(src_id)
        to_remove.add(srcinfo['filename'])
        self.count_src_refs.pop(src_id)

    def _remove_binpkg(self, pkg_name: str, to_remove: set,
                       ignore_missing: bool = False):
        """
        Remove binary package from index and stage package file removal. If it
        were the last package referencing a source package, the source package
        will be removed as well.
        """
        binpkg = self.binindex.pop(pkg_name, None)
        if not binpkg:
            if not ignore_missing:
                raise ValueError(f'package {pkg_name} not present')
            return

        to_remove.add(binpkg.filename)
        src_id = binpkg.srcid()
        self.count_src_refs[src_id] -= 1
        # If associated source package has no binary package remove it
        if self.count_src_refs[src_id] == 0:
            self._remove_srcpkg(src_id, to_remove)

    def _remove_src_and_bin_pkgs(self, src_id: str, to_remove: set):
        # Remove all binary packages associated to the source package. The
        # associated source package will be removed upon the last binary
        # package removal
        for binpkg in list(self.binindex.values()):
            if binpkg.srcid() == src_id:
                self._remove_binpkg(binpkg.name, to_remove)

    def _prepare_upload(self, manifest: dict, to_remove: set, to_add: set):
        """
        Adds to the binary-index and to the source-index of the repository the
        packages uploaded in the repository.

        Puts in a set the binary packages (files .mpk) that HAVE TO be added
        to the repository. The binary packages that are not needed anymore
        (because they have been replaced by a new version) are placed in
        another set. These two sets permit to remember which packages MUST be
        added and which must be removed after we are sure that the upload will
        be a success.

        Each binary package possesses a pointer towards a source. When a binary
        package is added, the number of references pointing to its source
        package is incremented. When a binary package SHOULD be removed, the
        number of references pointing to its source package is decremented.
        When the number of references of a source package reaches 0, it is
        added to the set of packages that MUST be removed once we are sure that
        the upload will be a success. In that case the source-index should also
        be removed from this package.

        Args:
            manifest: dictionary of the manifest file uploaded by the user.
            to_remove: set to fill with packages that MUST be removed once we
                       are sure that the upload will be a success.
            to_add: set to fill with packages that MUST be removed once we are
                    sure that the upload will be a success.
        """
        # add src entry
        # source id is name_srcsha256
        src_id = _srcid(manifest['name'], manifest['source']['sha256'])
        self.srcindex[src_id] = {
            'name': manifest['name'],
            'filename': manifest['source']['file'],
            'sha256': manifest['source']['sha256'],
            'size': manifest['source']['size'],
            'version': manifest['version']
        }
        to_add.add(manifest['source']['file'])

        # Add binary package entries
        for pkg_name, pkginfo in list(manifest['binpkgs'][self.arch].items()):
            self._check_hash(pkginfo)
            self.count_src_refs[src_id] += 1
            to_add.add(pkginfo['file'])

            # Remove previous binary package entry if any
            self._remove_binpkg(pkg_name, to_remove, ignore_missing=True)

            # Add new binary package entry
            mpk_path = os.path.join(self.working_dir, pkginfo['file'])
            binpkg = _BinPkg.load(mpk_path)
            binpkg.update({'filename': pkginfo['file'],
                           'size': pkginfo['size'],
                           'sha256': pkginfo['sha256']})

            self.binindex[binpkg.name] = binpkg

    def stage_upload(self, manifest_file: str,
                     remove_upload: bool = False) -> bool:
        """
        This function modifies the stagged changes to reflect a manifest upload

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
            remove_upload: if true, files (referenced packages and manifest)
                           are removed from upload dir.

        Returns:
            True if the modifications to the repository have been successfully
            stagged to the upcoming change. Otherwise false is returned.
        """
        try:
            manifest = yaml_load(manifest_file)
            mv_op = os.replace if remove_upload else shutil.copy
            self._mv_files_working_dir(manifest_file, manifest, mv_op)
            self._check_hash(manifest['source'])

            if self.arch not in manifest['binpkgs']:
                raise ValueError('Missing arch {self.arch} in {manifest_file}')

            # Remove previous source and binary package if matching source name
            # and version exist: this means that a rebuild is buing uploaded,
            # hence we need to remove the previous build
            name = manifest['name']
            version = manifest['version']
            for src_id in self._matching_srcids(srcname=name, version=version):
                self._remove_src_and_bin_pkgs(src_id, self.to_remove)

            # Update the binary-index and the source-index with the new
            # packages and upload binary packages and remove the binary
            # packages that are not needed anymore
            self._prepare_upload(manifest, self.to_remove, self.to_add)
        except (KeyError, IOError, ValueError) as error:
            self.logger.error(error)
            return False

        self._dump_indexes_working_dir(self.to_add)

        # Don't remove files that are uploaded (ie, overwritten)
        self.to_remove.difference_update(self.to_add)

        return True

    def stage_remove_matching_src(self, name: Optional[str] = None,
                                  version: Optional[str] = None,
                                  srcsha: Optional[str] = None) -> bool:
        """
        This function modifies the stagged changes to reflect a source package
        removal. When a source package is removed, all of its associated Binary
        Package are removed as well.

        Args:
            name: source package name
            version: version of source package
            srcsha: srcsha256 of source package

        Returns:
            True if the modifications to the repository have been successfully
            stagged to the upcoming change. Otherwise false is returned.
        """
        try:
            if not name and not version and not srcsha:
                raise ValueError('At least one option must be supplied')

            matchin_set = self._matching_srcids(srcname=name,
                                                version=version,
                                                srcsha256=srcsha)
            if not matchin_set:
                raise ValueError('No source package match constraints')

            for src_id in matchin_set:
                self._remove_src_and_bin_pkgs(src_id, self.to_remove)
        except (KeyError, IOError, ValueError) as error:
            self.logger.error(error)
            return False

        self._dump_indexes_working_dir(self.to_add)
        return True

    def _mv_files_working_dir(self, manifest_file: str, manifest: dict,
                              mv_op: Callable[[str, str], None]):
        """
        This function moves the manifest file as well as all the packages
        described in the manifest file into the working directory.

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
            manifest: dictionary of the manifest file uploaded by the user.
            mv_op: move/copy function to use
        """
        upload_dir = os.path.dirname(manifest_file)

        # Move the manifest file from the upload dir to the working
        # directory
        manifest_basename = os.path.basename(manifest_file)
        destination = os.path.join(self.working_dir, manifest_basename)
        mv_op(manifest_file, destination)

        # Move the binary packages from the sas directory to the working
        # directory
        for pkg_info in manifest['binpkgs'][self.arch].values():
            source = os.path.join(upload_dir, pkg_info['file'])
            destination = os.path.join(self.working_dir, pkg_info['file'])
            mv_op(source, destination)

        # Move the source package from the upload_dir directory to the working
        # directory
        src_filename = manifest['source']['file']
        source = os.path.join(upload_dir, src_filename)
        destination = os.path.join(self.working_dir, src_filename)
        mv_op(source, destination)

    def try_handle_upload(self, manifest_file: str,
                          remove_upload: bool = True) -> bool:
        """
        This function tries to handle the packages upload of the user.

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
            remove_upload: if true, files (referenced packages and manifest)
                           are removed from upload dir.

        Returns:
            True if the repository has been successfully updated. Otherwise
            false is returned and repository would be reverted to its state
            just before the call.
        """
        self.logger.info('Checking {}'.format(manifest_file))
        self.begin_changes()

        if not self.stage_upload(manifest_file, remove_upload):
            self.rollback_changes()
            self.logger.error("Error, revert data processing")
            return False

        self.commit_changes()
        self.logger.info('Data proceeded successfully')
        return True
