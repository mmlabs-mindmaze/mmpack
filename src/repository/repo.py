# pylint: disable=logging-format-interpolation
"""
Module provinding helpers.
- It provides a helper to check that all the information required in a manifest
are present.
- It provides a helper to check that the data provided by a user are coherent
with the information given in the manifest.
- It provides a helper to replace the files upload with the old files (if any)
"""

import logging
import logging.handlers
import os
import shutil
import tarfile
import yaml
from mmpack_build.common import yaml_serialize, yaml_load, sha256sum

RELPATH_BINARY_INDEX = 'binary-index'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'tmp'
PREFIX_LOG_FILE = '/var/log'
SUFFIX_LOG_FILE = 'mmpack-repo.log'


def _init_logger(architecture: str, repo_dir: str) -> logging.Logger:
    """
    Init logger to print to *log_file*.
    Files rotate every day, and are kept for a month

    Args:
        architecture: architecture on which the packages can be deployed.
        repo_dir: directory of the repository (where all the packages for a 
                  specific architecture are stored)
    """
    log_file = os.path.join(PREFIX_LOG_FILE, architecture)
    log_file = os.path.join(log_file, SUFFIX_LOG_FILE)
    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    logger = logging.getLogger(repo_dir + '-mmpack-repo')
    logger.addHandler(log_handler)
    logger.setLevel(logging.INFO)

    return logger


def _check_hash(package: dict):
    """
    Check that the hash provided in a dictionary representing a package is
    true: there exists effectively a file with the name of the package with
    this sha256.

    Note that to really test the coherence between the manifest and a package,
    we normally have to test that the information in the dictionary provided by
    the keys 'file', 'size' and 'sha256' are true: there exists a file
    respecting all these properties. However we can only test the sha256 and
    everything else is done freely:
    - the field 'file' will be checked while calling the function sha256sum
    which will raise an exception in case the path of the package is not a
    file.
    - We assume that if the sha256 is correct then the size should also be
    correct.

    Args:
        package: dictionary representing a package.
    """
    filename = package['file']
    filepath = os.path.join(RELPATH_WORKING_DIR, filename)

    if sha256sum(filepath) != package['sha256']:
        raise KeyError


class Repo:
    """
    This class model a repository.
    """
    # pylint: disable=too-few-public-methods

    def __init__(self, sas: str, repo: str, architecture: str):
        """
        Initializes a repository: retrieves the path of the upload directory
        (named sas_dir), and of the directory containing all the packages
        (named repo_dir), retrieves the data under the form of dictionary of
        the repository binary-index and of the repository source-index, creates
        a logger, retrieves the architecture on which the packages of the
        repository can be deployed, and registers the number of binary
        packages referencing each source package.

        Args:
            sas: the directory where the packages and the manifest file are
                 uploaded.
            repo: the directory where the packages, the binary-index and
                       the source-index are stored.
            architecture: architecture on which the packages of the repository
                          can be deployed.
        """
        self.sas_dir = sas
        self.repo_dir = repo
        self.logger = init_logger(architecture, repo)

        self.arch = architecture

        # binindex: dictionary of the packages mpk present on the database
        self.binindex = yaml_load(os.path.join(repo, RELPATH_BINARY_INDEX))
        if self.binindex is None:
            self.binindex = dict()
        # srcindex: dictionary of the sources present on the database
        self.srcindex = yaml_load(os.path.join(repo, RELPATH_SOURCE_INDEX))
        if self.srcindex is None:
            self.srcindex = dict()

        # count_src_refs: dictionary where the keys correspond to hash of
        # source packages and the values correspond to the number of binary
        # packages referencing the key.
        self.count_src_refs = dict()
        for pkg_info in self.binindex.values():
            srcsha = pkg_info['srcsha256']
            old_nb_refs = self.count_src_refs.get(srcsha, 0)
            self.count_src_refs[srcsha] = old_ne_refs + 1

    def _is_manifest_and_data_coherent(self, manifest: dict):
        """
        Check that the information provided in a dictionary are coherent: there
        exist actual files having the characteristic provided by the keys of
        the dictionary.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        _check_hash(manifest['source'])

        for pkg_info in manifest['binpkgs'][self.arch].values():
            _check_hash(pkg_info)

    def _effective_upload(self, to_add: set, to_remove: set):
        for filename in to_add:
            source = os.path.join(RELPATH_WORKING_DIR, filename)
            destination = os.path.join(self.repo_dir, filename)
            shutil.move(source, destination)

        for filename in to_remove:
            file_path = os.path.join(self.repo_dir, filename)
            os.remove(file_path)

    def _dump_cpy_indexes_working_dir(self, cp_binind: dict, cp_srcind: dict):
        srcindex_file = os.path.join(RELPATH_WORKING_DIR, RELPATH_SOURCE_INDEX)
        yaml_serialize(cp_srcind, srcindex_file, True)
        binindex_file = os.path.join(RELPATH_WORKING_DIR, RELPATH_BINARY_INDEX)
        yaml_serialize(cp_binind, binindex_file, True)

        self.srcindex = cp_srcind
        self.binindex = cp_binind

        dest_srcindex_file = os.path.join(self.repo_dir, RELPATH_SOURCE_INDEX)
        shutil.move(srcindex_file, dest_srcindex_file)
        dest_binindex_file = os.path.join(self.repo_dir, RELPATH_BINARY_INDEX)
        shutil.move(binindex_file, dest_binindex_file)

    def _prepare_update_indexes_repo(self, manifest: dict, cpy_binindex: dict,
                                     cpy_srcindex: dict):
        """
        Add to the binary-index and the source-index of the repository the
        packages uploaded in the repository. And then write an updated version
        of the binary-index file and of the source-index file in the
        repository.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        # Update source-index
        cpy_srcindex[manifest['source']['sha256']] = {
            'filename': manifest['source']['file'],
            'size': manifest['source']['size'],
            'version': manifest['version']}

        # Update binary-index
        for pkg_name, pkg_info in manifest['binpkgs'][self.arch].items():
            pkg_filename = pkg_info['file']
            pkg_path = os.path.join(self.repo_dir, pkg_filename)

            with tarfile.open(pkg_path, 'r:*') as mpk:
                stream = mpk.extractfile('./MMPACK/info').read()
                buf = yaml.safe_load(stream)
                buf[pkg_name].update({'filename': pkg_filename,
                                      'size': pkg_info['size'],
                                      'sha256': pkg_info['sha256']})

                cpy_binindex[pkg_name] = buf[pkg_name]

    def _prepare_upload_binary_packages(self, manifest: dict, to_remove: set,
                                        to_add: set):
        """
        Add the binary packages (files .mpk) in the repository. If some files
        already exist in the repository with same names, they are replaced.

        The binary packages that are not needed anymore (because they have been
        replaced by a new version) are placed in a set. This list permits to
        remember which packages MUST be removed after the whole upload is over.

        Each binary package possesses a pointer towards a source. When we
        remove a binary package, we decrement the number of references pointing
        to its source package. When the number of references of a source
        package reaches 0, we MUST remove this package after the whole upload
        is over.

        Args:
            count_src_refs: counter of references on the source packages.
            manifest: dictionary of the manifest file uploaded by the user.
            to_remove: set to fill with packages that MUST be removed after the
                       upload is over
        """
        for pkg_name, pkg_info in manifest['binpkgs'][self.arch].items():
            pkg_filename = pkg_info['file']

            if (pkg_name in self.binindex
                    and self.binindex[pkg_name]['filename'] != pkg_filename):
                to_remove.add(self.binindex[pkg_name]['filename'])
                srcsha256 = self.binindex[pkg_name]['srcsha256']
                nb_refs = self.count_src_refs.get(srcsha256) - 1
                self.count_src_refs[srcsha256] = nb_refs
                if nb_refs == 0:
                    to_remove.add(self.srcindex[srcsha256]['filename'])

            to_add.add(pkg_filename)

    def _upload(self, manifest: dict):
        """
        This function proceeds to the upload: upload the different packages
        (source and binary), update the indexes of the repository, and
        remove the packages not needed anymore.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        self._is_manifest_and_data_coherent(manifest)

        to_add = set()
        # Prepare upload of the source package
        to_add.add(manifest['source']['file'])
        # Upload binary packages and remove the binary packages that are not
        # needed anymore
        to_remove = set()
        self._prepare_upload_binary_packages(manifest, to_remove, to_add)

        # Update the binary-index and the source-index with the new packages
        cpy_binindex = self.binindex.copy()
        cpy_srcindex = self.srcindex.copy()
        self._prepare_update_indexes_repo(manifest, cpy_binindex, cpy_srcindex)

        # Remove the files that are not needed anymore
        self._dump_cpy_indexes_working_dir(cpy_binindex, cpy_srcindex)

    def _mv_files_working_dir(self, manifest_file: str, manifest: dict):
        # Move the manifest file from the sas directory to the working
        # directory
        manifest_basename = os.path.basename(manifest_file).split('_')[0]
        destination = os.path.join(RELPATH_WORKING_DIR, manifest_basename)
        shutil.move(manifest_file, destination)

        # Move the binary packages from the sas directory to the working
        # directory
        for pkg_info in manifest['binpkgs'][self.arch].values():
            source = os.path.join(self.sas_dir, pkg_info['file'])
            destination = os.path.join(RELPATH_WORKING_DIR, pkg_info['file'])
            shutil.move(source, destination)

        # Move the source package from the sas directory to the working
        # directory
        src_filename = manifest['source']['file']
        source = os.path.join(self.sas_dir, src_filename)
        destination = os.path.join(RELPATH_WORKING_DIR, src_filename)
        shutil.move(source, destination)

    def try_upload_files(self, manifest_file: str):
        """
        This function tries to upload the packages of the user.

        Args:
            archive_name: path through the archive containing the packages that
                          the user wants to upload.
        """
        try:
            os.mkdir(RELPATH_WORKING_DIR)
            self.logger.info('Checking {}'.format(manifest_file))

            manifest = yaml_load(manifest_file)
            self._mv_files_working_dir(manifest_file, manifest)
            self._upload(manifest)
            self.logger.info('Data proceeded successfully')

        except (KeyError, IOError) as exception:
            self.logger.error('Error: {}'.format(exception.strerror))
        finally:
            shutil.rmtree(RELPATH_WORKING_DIR)
