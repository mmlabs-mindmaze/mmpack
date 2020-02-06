"""
Module provinding helpers.
- It provides a helper to check that all the information required in a manifest
are present.
- It provides a helper to check that the data provided by a user are coherent
with the information given in the manifest.
- It provides a helper to replace the files upload with the old files (if any)
"""

import os
import glob
import signal
import shutil
import tarfile
import atexit
import yaml
from mmpack_build.common import yaml_serialize
from mmpack_build.common import sha256sum

RELPATH_BINARY_INDEX = 'binary-index'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'tmp'

class Repo():
    """
    This class model a repository.
    """

    def __init__(self, directory: str):
        """
        Initialize a repository: retrieve the path of the binary-index, and of
        the source-index, retrieve the data under the form of dictionnary of the
        binary-index and of the source-index.

        Args:
            directory: the directory where the packages, the binary-index and
                       the source-index are stored
        """
        self.db_dir = directory

        # bin_ind: path through the binary index
        self.bin_ind = os.path.join(directory, RELPATH_BINARY_INDEX)
        # src_ind: path through the source index
        self.src_ind = os.path.join(directory, RELPATH_SOURCE_INDEX)
        # data_bin_ind: dictionnary of the packages mpk present on the database
        with open(self.bin_ind, 'r') as bin_file:
            self.data_bin_ind = yaml.safe_load(bin_file)
            if self.data_bin_ind is None:
                self.data_bin_ind = dict()
        # data_src_ind: dictionnary of the sources present on the database
        with open(self.src_ind, 'r') as source_file:
            self.data_src_ind = yaml.safe_load(source_file)
            if self.data_src_ind is None:
                self.data_src_ind = dict()
        # working directory
        self.working_dir = RELPATH_WORKING_DIR
        # list of the files to suppress
        self.list_to_remove = list()


    def _is_main_key_and_data_coherent(self, data_manifest: dict, main_key: str) -> bool:
        """
        Check that the information provided in a dictionnary by a given key are
        true (the files information given by the key are effectively true: there
        exists a file with the same information in the prefix)

        Args:
            data_manifest: dictionnary of the manifest file uploaded by the user
            main_key: the key from which the information are checked
        """
        filename = data_manifest[main_key]['file']
        filepath = os.path.join(self.working_dir, filename)

        return ((os.path.isfile(filepath))
                and
                (os.path.getsize(filepath) == data_manifest[main_key]['size'])
                and (sha256sum(filepath) == data_manifest[main_key]['sha256']))


    def _is_manifest_and_data_coherent(self, data_manifest: dict) -> bool:
        """
        Check that the information provided in a dictionnary are coherent: there
        exist actual files having the characteristic provided by the keys of the
        dictionnary.

        Args:
            data_manifest: dictionnary of the manifest file uploaded by the user
        """
        try:
            if not 'name' in data_manifest or not 'version' in data_manifest:
                return False

            if not self._is_main_key_and_data_coherent(data_manifest, 'source'):
                return False

            for arch in data_manifest['binpkgs']:
                for key in data_manifest['binpkgs'][arch]:
                    main_key = data_manifest['binpkgs'][arch]
                    if not self._is_main_key_and_data_coherent(main_key, key):
                        return False

            return True
        except KeyError:
            return False


    def _suppress_old_packages(self):
        """
        Suppress all the packages of the repository that have been replaced by
        new versions (see function _add_or_replace_files)
        """
        for filename in self.list_to_remove:
            file_path = os.path.join(self.db_dir, filename)
            os.remove(file_path)

        del self.list_to_remove[:]


    def _update_indexes_repo(self, data_manifest: dict):
        """
        Add to the binary-index and the source-index of the repository the
        packages uploaded in the repository. And then write an updated version
        of the binary-index file and of the source-index file in the repository.

        Args:
            data_manifest: dictionnary of the manifest file uploaded by the user
        """
        # Update source-index
        self.data_src_ind[data_manifest['name']] = {
            'filename': data_manifest['source']['file'],
            'size': data_manifest['source']['size'],
            'sha256': data_manifest['source']['sha256'],
            'version':data_manifest['version']}

        # Update binary-index
        for arch in data_manifest['binpkgs']:
            for key in data_manifest['binpkgs'][arch]:
                pkg_filename = data_manifest['binpkgs'][arch][key]['file']
                pkg_path = os.path.join(self.db_dir, pkg_filename)

                with tarfile.open(pkg_path, 'r:*') as mpk:
                    stream = mpk.extractfile('./MMPACK/info').read()
                    buf = yaml.safe_load(stream)
                    buf[key].update({
                        'filename': pkg_filename,
                        'size': data_manifest['binpkgs'][arch][key]['size'],
                        'sha256': data_manifest['binpkgs'][arch][key]['sha256']
                        })

                    self.data_bin_ind[key] = buf[key]

        # Dump source-index and binary-index
        yaml_serialize(self.data_src_ind, self.src_ind, 1)
        yaml_serialize(self.data_bin_ind, self.bin_ind, 1)


    def _add_or_replace_files(self, data_manifest: dict):
        """
        Add the files (mpk and tar.xz) uploaded by the user in the repository.
        If some files already exist with the same name, they are replaced. This
        function also update the binary-index and source-index of the
        repository.

        Args:
            data_manifest: dictionnary of the manifest file uploaded by the user
        """
        # In case there exists a source package identified by the same name in
        # the manifest and in the source-index, then we check if the filenames
        # associated to these source packages are also similar. If this is not
        # the case, then we just save the filename of the source package
        # registered in the source-index, to remove it latter (after all the
        # actions of the upload have been performed). In any case the source
        # package registered in the manifest will be uploaded in the packages
        # repository.
        pkg_name = data_manifest['name']
        pkg_filename = data_manifest['source']['file']

        if (pkg_name in self.data_src_ind
                and self.data_src_ind[pkg_name]['filename'] != pkg_filename):
            self.list_to_remove.append(self.data_src_ind[pkg_name]['filename'])
            del self.data_src_ind[pkg_name]

        source = os.path.join(self.working_dir, pkg_filename)
        destination = os.path.join(self.db_dir, pkg_filename)
        shutil.move(source, destination)

        # We perform the same action for the binary packages
        for arch in data_manifest['binpkgs']:
            for key in data_manifest['binpkgs'][arch]:

                pkg_name = key
                pkg_filename = data_manifest['binpkgs'][arch][key]['file']

                if (pkg_name in self.data_bin_ind
                        and
                        self.data_bin_ind[pkg_name]['filename'] != pkg_filename):

                    to_remove = self.data_bin_ind[pkg_name]['filename']
                    self.list_to_remove.append(to_remove)
                    del self.data_bin_ind[pkg_name]

                source = os.path.join(self.working_dir, pkg_filename)
                destination = os.path.join(self.db_dir, pkg_filename)
                shutil.move(source, destination)

        # Update the binary-index and the source-index with the new packages
        self._update_indexes_repo(data_manifest)


    def clean_upload(self, archive_name: str):
        """
        This function cleans the archive uploaded by the user and the residual
        files created to handle the upload.

        Args:
            archive_name: name of the archive uploaded by the user
        """
        # suppress the residual files
        shutil.rmtree(self.working_dir)
        os.remove(archive_name)


    def try_upload(self, archive_name: str):
        """
        This function try to upload the packages of the user.

        Args:
            archive_name: path through the archive containing the packages that
                          the user wants to upload.
        """
        print('Checking', archive_name)

        atexit.register(self.clean_upload)

        os.mkdir(self.working_dir)

        with tarfile.open(archive_name, 'r:*') as tar:
            tar.extractall(self.working_dir)

            manifest = os.path.join(RELPATH_WORKING_DIR, '*.mmpack-manifest')
            if not glob.glob(manifest):
                print('Abort: No manifest file found')

            for manifest_path in glob.glob(manifest):
                if os.path.isfile(manifest_path):

                    with open(manifest_path, 'r') as manifest_file:
                        data_manifest = yaml.safe_load(manifest_file)

                        if not self._is_manifest_and_data_coherent(data_manifest):
                            print('Abort: manifest is not coherent with data')
                            return

                        self._add_or_replace_files(data_manifest)

                        # Remove the files that are not needed anymore
                        self._suppress_old_packages()
                        print('Data proceeded successfully')


    signal.signal(signal.SIGINT, clean_upload)
