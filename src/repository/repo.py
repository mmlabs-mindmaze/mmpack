# pylint: disable=logging-format-interpolation
"""
Module provinding helpers.
- It provides a helper to check that all the information required in a manifest
are present.
- It provides a helper to check that the data provided by a user are coherent
with the information given in the manifest.
- It provides a helper to replace the files upload with the old files (if any)
"""

from collections import Counter
from hashlib import sha256
import logging
import logging.handlers
import os
import shutil
import tarfile
from typing import Union, Callable
import yaml

RELPATH_BINARY_INDEX = 'binary-index'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'working_dir'
LOG_FILE = 'mmpack-repo.log'


# The functions sha256sum, yaml_serialize, and yaml_load
# are functions that are extracted from mmpack-build/common.py. There are
# copied in this file in order to avoid the import of the module common and
# therefore in order to avoid to have a dependance.

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
    sha.update(open(filename, 'rb').read())
    hexdig = sha.hexdigest()

    return hexdig


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    return yaml.load(open(filename, 'rb').read(), Loader=yaml.BaseLoader)


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


def _info_mpk(pkg_path: str) -> dict:
    """
    This function reads a mpk file and returns a dictionary containing the
    information read.

    Args:
        pkg_path: the path through the mkp file to read.
    """
    with tarfile.open(pkg_path, 'r:*') as mpk:
        buf = mpk.extractfile('./MMPACK/info').read()
        return yaml.safe_load(buf)


def file_serialize(index: dict, filename: str):
    """
    This function serializes a dictionnary into a flat structure in a file.

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

    for line in open(filename, 'r'):
        if not line.strip():
            src_id = _srcid(entry['name'], entry['sha256'])
            srcindex[src_id] = entry.copy()
            continue

        key, value = line.split(': ')
        entry[key] = value.rstrip('\n')

    return srcindex


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

        binindex_file = os.path.join(abs_path_repo, RELPATH_BINARY_INDEX)
        srcindex_file = os.path.join(abs_path_repo, RELPATH_SOURCE_INDEX)
        if not os.path.isfile(binindex_file):
            open(binindex_file, 'w+')

        if not os.path.isfile(srcindex_file):
            open(srcindex_file, 'w+')

        self.repo_dir = abs_path_repo
        self.working_dir = os.path.abspath(os.path.join(abs_path_repo,
                                                        RELPATH_WORKING_DIR))
        self.logger = _init_logger(os.path.join(self.repo_dir, LOG_FILE))

        self.arch = architecture

        # binindex: dictionary of the packages mpk present on the database
        self.binindex = yaml_load(binindex_file)
        if self.binindex is None:
            self.binindex = dict()
        # srcindex: dictionary of the sources present on the database
        self.srcindex = file_load(srcindex_file)
        if self.srcindex is None:
            self.srcindex = dict()

        # count_src_refs: dictionary where the keys correspond to hash of
        # source packages and the values correspond to the number of binary
        # packages referencing the key.
        self.count_src_refs = Counter()
        for pkg_info in self.binindex.values():
            src_id = _srcid(pkg_info['source'], pkg_info['srcsha256'])
            self.count_src_refs[src_id] += 1

    def yaml_serialize(self, obj: Union[list, dict], filename: str,
                       use_block_style: bool = False) -> None:
        """
        Save object as yaml file of given name
        """
        default_flow_style = None
        if use_block_style:
            default_flow_style = False

        with open(filename, 'w+', newline='\n') as outfile:
            yaml.dump(obj, outfile,
                      default_flow_style=default_flow_style,
                      allow_unicode=True,
                      indent=4)
        self.logger.info('wrote {0}'.format(filename))

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

    def _commit_upload(self, to_add: set, to_remove: set):
        """
        Moves the packages asked to be uploaded from the working directory to
        the repository. Removes the packages of the repository that are not
        needed anymore.

        Args:
            to_add: set of packages that have to be added to the repository.
            to_remove: set of packages of the repository that have to be
                       removed.
        """
        to_remove.difference_update(to_add)
        for filename in to_remove:
            self.logger.info('Remove {} from repository'.format(filename))
            file_path = os.path.join(self.repo_dir, filename)
            os.remove(file_path)

        for filename in to_add:
            self.logger.info('Add {} in repository'.format(filename))
            source = os.path.join(self.working_dir, filename)
            destination = os.path.join(self.repo_dir, filename)
            os.replace(source, destination)

    def _dump_indexes_working_dir(self, to_add: set):
        """
        Writes the updated binary-index and source-index into the working
        directory. If no problem occurs while writting these files, then this
        function updates the dictionaries of the repository (referencing the
        data of the binary-index and of the source-index) and moves the
        binary-index and source-index previously written into the repository.

        Args:
            to_add: set to fill with packages that MUST be removed once we are
                    sure that the upload will be a sucess.
        """
        srcindex_file = os.path.join(self.working_dir, RELPATH_SOURCE_INDEX)
        file_serialize(self.srcindex, srcindex_file)
        binindex_file = os.path.join(self.working_dir, RELPATH_BINARY_INDEX)
        self.yaml_serialize(self.binindex, binindex_file, True)

        to_add.add(RELPATH_SOURCE_INDEX)
        to_add.add(RELPATH_BINARY_INDEX)

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
                    sure that the upload will be a sucess.
        """
        # add src entry
        self._check_hash(manifest['source'])
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
        for pkg_name, pkginfo in manifest['binpkgs'][self.arch].items():
            self._check_hash(pkginfo)
            self.count_src_refs[src_id] += 1
            to_add.add(pkginfo['file'])

            # Remove previous binary package entry if any
            prev_pkginfo = self.binindex.get(pkg_name)
            if prev_pkginfo:
                to_remove.add(prev_pkginfo['filename'])
                prev_id = _srcid(prev_pkginfo['source'],
                                 prev_pkginfo['srcsha256'])
                self.count_src_refs[prev_id] -= 1
                # If previous source package has not binary package remove it
                if self.count_src_refs[prev_id] == 0:
                    to_remove.add(self.srcindex[prev_id]['filename'])
                    self.srcindex.pop(prev_id)

            # Add new binary package entry
            buf = _info_mpk(os.path.join(self.working_dir, pkginfo['file']))
            buf[pkg_name].update({'filename': pkginfo['file'],
                                  'size': pkginfo['size'],
                                  'sha256': pkginfo['sha256']})

            self.binindex.update(buf)

    def _handle_upload(self, manifest: dict):
        """
        This function handles the upload.

        Args:
            manifest: dictionary of the manifest file uploaded by the user.
        """
        to_add = set()
        to_remove = set()
        # Update the binary-index and the source-index with the new packages
        # and upload binary packages and remove the binary packages that are
        # not needed anymore
        self._prepare_upload(manifest, to_remove, to_add)

        # Remove the files that are not needed anymore
        self._dump_indexes_working_dir(to_add)
        self._commit_upload(to_add, to_remove)

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
                          remove_upload: bool = True):
        """
        This function tries to handle the packages upload of the user.

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
            remove_upload: if true, files (referenced packages and manifest)
                           are removed from upload dir.
        """
        try:
            backup_srcindex = self.srcindex.copy()
            backup_binindex = self.binindex.copy()
            backup_counter = self.count_src_refs.copy()

            os.mkdir(self.working_dir)
            self.logger.info('Checking {}'.format(manifest_file))

            manifest = yaml_load(manifest_file)
            mv_op = os.replace if remove_upload else shutil.copy
            self._mv_files_working_dir(manifest_file, manifest, mv_op)
            self._handle_upload(manifest)
            self.logger.info('Data proceeded successfully')

        except (KeyError, IOError, ValueError):
            self.logger.error("Error, revert data processing")
            self.srcindex = backup_srcindex
            self.binindex = backup_binindex
            self.count_src_refs = backup_counter
        finally:
            shutil.rmtree(self.working_dir)
