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
from mmpack_build.common import yaml_serialize, yaml_load, sha256sum

RELPATH_BINARY_INDEX = 'binary-index'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'tmp'

class Repo:
    """
    This class model a repository.
    """

    def __init__(self, directory: str, architecture: str):
        """
        Initialize a repository: retrieve the path of the binary-index, and of
        the source-index, retrieve the data under the form of dictionary of the
        binary-index and of the source-index.

        Args:
            directory: the directory where the packages, the binary-index and
                       the source-index are stored
            architecture: architecture that the server handles
        """
        os.mkdir(RELPATH_WORKING_DIR)

        self.arch = architecture

        # binindex: dictionary of the packages mpk present on the database
        self.binindex = yaml_load(os.path.join(directory, RELPATH_BINARY_INDEX))
        if self.binindex is None:
            self.binindex = dict()
        # srcindex: dictionary of the sources present on the database
        self.srcindex = yaml_load(os.path.join(directory, RELPATH_SOURCE_INDEX))
        if self.srcindex is None:
            self.srcindex = dict()


    def deinit():
        shutil.rmtree(RELPATH_WORKING_DIR)

    def _check_hash(self, data_package: dict) -> bool:
        """
        Check that the hash provided in a dictionary for a package is true:
        there exists effectively a file with the name of the package with this
        sha256.

        Note that to really test the coherence between the manifest and a
        package, we normally have to test that the information in the
        dictionary provided by the keys 'file', 'size' and 'sha256' are true:
        there exists a file respecting all these properties. However we can only
        test the sha256 and everything else is done freely:
            - the field 'file' will be checked while calling the function
            sha256sum which will raise an exception in case the path of the
            package is not a file.
            - We assume that if the sha256 is correct then the size should also
            be correct.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
            main_key: the key from which the information are checked
        """
        filename = data_package['file']
        filepath = os.path.join(RELPATH_WORKING_DIR, filename)

        return sha256sum(filepath) == data_package['sha256']


    def _is_manifest_and_data_coherent(self, manifest: dict) -> bool:
        """
        Check that the information provided in a dictionary are coherent: there
        exist actual files having the characteristic provided by the keys of the
        dictionary.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        try:
            if not 'name' in manifest or not 'version' in manifest:
                return False

            if not self._check_hash(manifest['source']):
                return False

            for arch in manifest['binpkgs']:
                for key in manifest['binpkgs'][arch]:
                    if not self._check_hash(manifest['binpkgs'][arch][key]):
                        return False

            return True
        except:
            return False


    def _suppress_old_packages(self, list_to_remove: list):
        """
        Suppress all the packages of the repository that have been replaced by
        new versions (see function _add_or_replace_files).

        Args:
            list_to_remove: list of the packages that MUST be removed
        """
        for filename in self.list_to_remove:
            file_path = os.path.join(self.db_dir, filename)
            os.remove(file_path)


    def _update_indexes_repo(self, manifest: dict):
        """
        Add to the binary-index and the source-index of the repository the
        packages uploaded in the repository. And then write an updated version
        of the binary-index file and of the source-index file in the repository.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        # Update source-index
        self.srcindex[manifest['name']] = {
            'filename': manifest['source']['file'],
            'size': manifest['source']['size'],
            'sha256': manifest['source']['sha256'],
            'version':manifest['version']}

        # Update binary-index
        for arch in manifest['binpkgs']:
            for key in manifest['binpkgs'][arch]:
                pkg_filename = manifest['binpkgs'][arch][key]['file']
                pkg_path = os.path.join(self.db_dir, pkg_filename)

                with tarfile.open(pkg_path, 'r:*') as mpk:
                    stream = mpk.extractfile('./MMPACK/info').read()
                    buf = yaml.safe_load(stream)
                    buf[key].update({
                        'filename': pkg_filename,
                        'size': manifest['binpkgs'][arch][key]['size'],
                        'sha256': manifest['binpkgs'][arch][key]['sha256']
                        })

                    self.binindex[key] = buf[key]

        # Dump source-index and binary-index
        yaml_serialize(self.srcindex, self.srcindex_file, True)
        yaml_serialize(self.binindex, self.binindex_file, True)











#TODO: simplification of the code thanks to self.arch.

#TODO: separate the functions: make a function for the source packages and
#      for the binary packages.





    def _add_or_replace_source_packages(self, manifest: dict):
        #TODO: make a counter of reference and change and remove the source 
        # files according to this number
        pkg_name = manifest['name']
        pkg_filename = manifest['source']['file']
        list_to_remove = list()

        if (pkg_name in self.srcindex
                and self.srcindex[pkg_name]['filename'] != pkg_filename):
            list_to_remove.append(self.srcindex[pkg_name]['filename'])

        source = os.path.join(RELPATH_WORKING_DIR, pkg_filename)
        destination = os.path.join(self.db_dir, pkg_filename)
        shutil.move(source, destination)




    def _add_or_replace_binary_packages(self, manifest: dict):
 # Perform the same actions for the binary packages
        for arch in manifest['binpkgs']:
            for pkg_name in manifest['binpkgs'][arch]:

                pkg_filename = manifest['binpkgs'][arch][pkg_name]['file']

                if (pkg_name in self.binindex
                        and
                        self.binindex[pkg_name]['filename'] != pkg_filename):

                    to_remove = self.binindex[pkg_name]['filename']
                    list_to_remove.append(to_remove)

                source = os.path.join(RELPATH_WORKING_DIR, pkg_filename)
                destination = os.path.join(self.db_dir, pkg_filename)
                shutil.move(source, destination)




    def _add_or_replace_files(self, manifest: dict):
        """
        Add the files (mpk and tar.xz) uploaded by the user in the repository.
        If some files already exist with the same name, they are replaced. This
        function also update the binary-index and source-index of the
        repository. It also suppresses the files that are not needed anymore
        (because a new version has been uploaded).

        In case there exists a source package identified by the same name in
        the manifest and in the source-index, then we check if the filenames
        associated to these source packages are also similar. If this is not
        the case, then we just save the filename of the source package
        registered in the source-index, to remove it latter (after all the
        actions of the upload have been performed). In any case the source
        package registered in the manifest will be uploaded in the packages
        repository. The same actions are performed for the binary packages.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        # Perform the actions on the source package
        _add_or_replace_source_packages(manifest)
        _add_or_replace_binary_packages(manidest)
        
        
        
               # Update the binary-index and the source-index with the new packages
        self._update_indexes_repo(manifest)

        # Remove the files that are not needed anymore
        self._suppress_old_packages(list_to_remove)
        list_to_remove.clear()


    def _try_upload(self):
        manifest_file = os.path.join(RELPATH_WORKING_DIR, '*.mmpack-manifest')

        if len(glob.glob(manifest_file)) != 1:
            print('Abort: No manifest, or mulitple manifests found')
            return

        manifest = yaml_load(glob.glob(manifest_file)[0])

        if not self._is_manifest_and_data_coherent(manifest):
            print('Abort: manifest is not coherent with data')
            return

        self._add_or_replace_files(manifest)
        print('Data proceeded successfully')
    

    def upload(self, archive_name: str):
        """
        This function tries to upload the packages of the user.

        Args:
            archive_name: path through the archive containing the packages that
                          the user wants to upload.
        """
        try:
            print('Checking', archive_name)

            with tarfile.open(archive_name, 'r:*') as tar:
                tar.extractall(self.working_dir)

                os.remove(archive_name)

                self.try_upload()

        finally:
            self.deinit()
