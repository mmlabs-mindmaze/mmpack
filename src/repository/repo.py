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
from typing import Union
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

    logger = logging.getLogger(log_file)
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


class Repo:
    """
    This class model a repository.
    """
    # pylint: disable=too-few-public-methods
    # pylint: disable=too-many-instance-attributes

    def __init__(self, sas: str, repo: str, architecture: str):
        """
        Initializes a repository: retrieves the path of the upload directory
        (named sas_dir), the path of the working directory and the directory
        containing all the packages (named repo_dir), retrieves the data under
        the form of dictionary of the repository binary-index and of the
        repository source-index, creates a logger, retrieves the architecture
        on which the packages of the repository can be deployed, and registers
        the number of binary packages referencing each source package.

        Args:
            sas: the directory where the packages and the manifest file are
                 uploaded.
            repo: the directory where the packages, the binary-index and
                       the source-index are stored.
            architecture: architecture on which the packages of the repository
                          can be deployed.
        """
        abs_path = os.path.dirname(__file__)
        abs_path_repo = os.path.abspath(os.path.join(abs_path, repo))
        if not os.path.isdir(abs_path_repo):
            os.mkdir(abs_path_repo)

        binindex_file = os.path.join(abs_path_repo, RELPATH_BINARY_INDEX)
        srcindex_file = os.path.join(abs_path_repo, RELPATH_SOURCE_INDEX)
        if not os.path.isfile(binindex_file):
            open(binindex_file, 'w+')

        if not os.path.isfile(srcindex_file):
            open(srcindex_file, 'w+')

        self.sas_dir = sas
        self.repo_dir = abs_path_repo
        self.working_dir = os.path.abspath(os.path.join(abs_path,
                                                        RELPATH_WORKING_DIR))
        self.logger = _init_logger(os.path.abspath(os.path.join(self.repo_dir,
                                                                LOG_FILE)))

        self.arch = architecture

        # binindex: dictionary of the packages mpk present on the database
        self.binindex = yaml_load(binindex_file)
        if self.binindex is None:
            self.binindex = dict()
        # srcindex: dictionary of the sources present on the database
        self.srcindex = yaml_load(srcindex_file)
        if self.srcindex is None:
            self.srcindex = dict()

        # count_src_refs: dictionary where the keys correspond to hash of
        # source packages and the values correspond to the number of binary
        # packages referencing the key.
        self.count_src_refs = Counter()
        for pkg_info in self.binindex.values():
            srcsha = pkg_info['srcsha256']
            self.count_src_refs[srcsha] += 1

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

    def _is_manifest_and_data_coherent(self, manifest: dict):
        """
        Checks that the information provided in a dictionary are coherent:
        there exist actual files having the characteristic provided by the keys
        of the dictionary.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        self._check_hash(manifest['source'])

        for pkg_info in manifest['binpkgs'][self.arch].values():
            self._check_hash(pkg_info)

    def _effective_handle_upload(self, to_add: set, to_remove: set):
        """
        Moves the packages asked to be uploaded from the working directory to
        the repository. Removes the packages of the repository that are not
        needed anymore.

        Args:
            to_add: set of packages that have to be added to the repository.
            to_remove: set of packages of the repository that have to be
                       removed.
        """
        for filename in to_remove:
            self.logger.info('Remove {} from repository'.format(filename))
            file_path = os.path.join(self.repo_dir, filename)
            os.remove(file_path)

        for filename in to_add:
            self.logger.info('Add {} in repository'.format(filename))
            source = os.path.join(self.working_dir, filename)
            destination = os.path.join(self.repo_dir, filename)
            os.replace(source, destination)

    def _dump_cpy_indexes_working_dir(self, cp_binind: dict, cp_srcind: dict,
                                      to_add: set):
        """
        Writes the updated binary-index and source-index into the working
        directory. If no problem occurs while writting these files, then this
        function updates the dictionaries of the repository (referencing the
        data of the binary-index and of the source-index) and moves the
        binary-index and source-index previously written into the repository.

        Args:
            cp_binind: dictionary containing the updated information about the
                       binary packages present in the repository.
            cp_srcind: dictionary containing the updated information about the
                       source package present in the repository.
            to_add: set to fill with packages that MUST be removed once we are
                    sure that the upload will be a sucess.
        """
        srcindex_file = os.path.join(self.working_dir, RELPATH_SOURCE_INDEX)
        self.yaml_serialize(cp_srcind, srcindex_file, True)
        binindex_file = os.path.join(self.working_dir, RELPATH_BINARY_INDEX)
        self.yaml_serialize(cp_binind, binindex_file, True)

        self.srcindex = cp_srcind
        self.binindex = cp_binind

        to_add.add(RELPATH_SOURCE_INDEX)
        to_add.add(RELPATH_BINARY_INDEX)

    def _prepare_update_indexes_repo(self, manifest: dict, cpy_binindex: dict,
                                     cpy_srcindex: dict):
        """
        Adds to a copy of the binary-index and to a copy of the source-index of
        the repository the packages uploaded in the repository.

        Args:
            manifest: dictionary of the manifest file uploaded by the user.
            cpy_binindex: copy of the dictionary containing the information
                          of the binary-index file.
            cpy_srcindex: copy of the dictionary containing the information
                          of the source-index file.
        """
        # Update source-index
        cpy_srcindex[manifest['source']['sha256']] = {
            'name': manifest['name'],
            'filename': manifest['source']['file'],
            'size': manifest['source']['size'],
            'version': manifest['version']}

        # Update binary-index
        for pkg_name, pkg_info in manifest['binpkgs'][self.arch].items():
            pkg_filename = pkg_info['file']
            pkg_path = os.path.join(self.working_dir, pkg_filename)

            buf = _info_mpk(pkg_path)
            buf[pkg_name].update({'filename': pkg_filename,
                                  'size': pkg_info['size'],
                                  'sha256': pkg_info['sha256']})

            cpy_binindex[pkg_name] = buf[pkg_name]

    def _prepare_handle_upload_binary_packages(self, manifest: dict,
                                               to_remove: set, to_add: set,
                                               cpy_srcindex: dict):
        """
        Puts in a set the binary packages (files .mpk) that HAVE TO be added
        to the repository. The binary packages that are not needed anymore
        (because they have been replaced by a new version) are placed in
        another set. These two sets permit to remember which packages MUST be
        added and which must be removed after we are sure that the upload will
        be a success.

        Each binary package possesses a pointer towards a source. When a binary
        package SHOULD be removed, the number of references pointing to its
        source package is decremented. When the number of references of a
        source package reaches 0, it is added to the set of packages that MUST
        be removed once we are sure that the upload will be a success. In that
        case the source-index should also be removed from this package (the
        package is removed from the copy of the dictionary containing the
        information about the source packages present in the repository, and
        the actual removed will be effected when this copy will be written on
        the repository)

        Args:
            manifest: dictionary of the manifest file uploaded by the user.
            to_remove: set to fill with packages that MUST be removed once we
                       are sure that the upload will be a success.
            to_add: set to fill with packages that MUST be removed once we are
                    sure that the upload will be a sucess.
        """
        for pkg_name, pkg_info in manifest['binpkgs'][self.arch].items():
            pkg_filename = pkg_info['file']

            pkg_info = self.binindex.get(pkg_name)
            if pkg_info:
                to_remove.add(pkg_info['filename'])
                srcsha256 = pkg_info['srcsha256']
                self.count_src_refs[srcsha256] -= 1
                if self.count_src_refs[srcsha256] == 0:
                    to_remove.add(self.srcindex[srcsha256]['filename'])
                    cpy_srcindex.pop(srcsha256)

            to_add.add(pkg_filename)

    def _handle_upload(self, manifest: dict):
        """
        This function handles the upload.

        Args:
            manifest: dictionary of the manifest file uploaded by the user.
        """
        self._is_manifest_and_data_coherent(manifest)

        to_add = set()
        # Prepare upload of the source package
        to_add.add(manifest['source']['file'])
        # Upload binary packages and remove the binary packages that are not
        # needed anymore
        to_remove = set()
        cpy_binindex = self.binindex.copy()
        cpy_srcindex = self.srcindex.copy()
        self._prepare_handle_upload_binary_packages(manifest, to_remove,
                                                    to_add, cpy_srcindex)

        # Update the binary-index and the source-index with the new packages
        self._prepare_update_indexes_repo(manifest, cpy_binindex, cpy_srcindex)
        # Remove the files that are not needed anymore
        self._dump_cpy_indexes_working_dir(cpy_binindex, cpy_srcindex, to_add)
        self._effective_handle_upload(to_add, to_remove)

    def _mv_files_working_dir(self, manifest_file: str, manifest: dict):
        """
        This function moves the manifest file as well as all the packages
        described in the manifest file into the working directory.

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
            manifest: dictionary of the manifest file uploaded by the user.
        """
        # Move the manifest file from the sas directory to the working
        # directory
        manifest_basename = os.path.basename(manifest_file).split('_')[0]
        destination = os.path.join(self.working_dir, manifest_basename)
        os.replace(manifest_file, destination)

        # Move the binary packages from the sas directory to the working
        # directory
        for pkg_info in manifest['binpkgs'][self.arch].values():
            source = os.path.join(self.sas_dir, pkg_info['file'])
            destination = os.path.join(self.working_dir, pkg_info['file'])
            os.replace(source, destination)

        # Move the source package from the sas directory to the working
        # directory
        src_filename = manifest['source']['file']
        source = os.path.join(self.sas_dir, src_filename)
        destination = os.path.join(self.working_dir, src_filename)
        os.replace(source, destination)

    def try_handle_upload(self, manifest_file: str):
        """
        This function tries to handle the packages upload of the user.

        Args:
            manifest_file: path through the manifest file containing
                           information about the packages that the user wants
                           to upload.
        """
        try:
            os.mkdir(self.working_dir)
            self.logger.info('Checking {}'.format(manifest_file))

            manifest = yaml_load(manifest_file)
            self._mv_files_working_dir(manifest_file, manifest)
            self._handle_upload(manifest)
            self.logger.info('Data proceeded successfully')

        except (KeyError, IOError) as exception:
            self.logger.error('Error: {}'.format(exception.strerror))
        finally:
            shutil.rmtree(self.working_dir)
