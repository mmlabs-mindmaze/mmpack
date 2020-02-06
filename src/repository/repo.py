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
import shutil
import tarfile
import yaml
from mmpack_build.common import yaml_serialize, yaml_load, sha256sum

RELPATH_BINARY_INDEX = 'binary-index'
RELPATH_SOURCE_INDEX = 'source-index'
RELPATH_WORKING_DIR = 'tmp'


def deinit():
    """
    This method suppresses the directory where the upload have been handled.
    """
    shutil.rmtree(RELPATH_WORKING_DIR)


def _check_hash(package: dict) -> bool:
    """
    Check that the hash provided in a dictionary representing a package is true:
    there exists effectively a file with the name of the package with this
    sha256.

    Note that to really test the coherence between the manifest and a package,
    we normally have to test that the information in the dictionary provided by
    the keys 'file', 'size' and 'sha256' are true: there exists a file
    respecting all these properties. However we can only test the sha256 and
    everything else is done freely:
    - the field 'file' will be checked while calling the function sha256sum
    which will raise an exception in case the path of the package is not a file.
    - We assume that if the sha256 is correct then the size should also be
    correct.

    Args:
        package: dictionary representing a package.
    """
    filename = package['file']
    filepath = os.path.join(RELPATH_WORKING_DIR, filename)

    return sha256sum(filepath) == package['sha256']


#pylint: disable=too-few-public-methods
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

        self.curr_dir = directory
        self.arch = architecture

        # binindex: dictionary of the packages mpk present on the database
        self.binindex = yaml_load(os.path.join(directory, RELPATH_BINARY_INDEX))
        if self.binindex is None:
            self.binindex = dict()
        # srcindex: dictionary of the sources present on the database
        self.srcindex = yaml_load(os.path.join(directory, RELPATH_SOURCE_INDEX))
        if self.srcindex is None:
            self.srcindex = dict()


    def _is_manifest_and_data_coherent(self, manifest: dict) -> bool:
        """
        Check that the information provided in a dictionary are coherent: there
        exist actual files having the characteristic provided by the keys of the
        dictionary.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        try:
            if (manifest.get('name') is None or manifest.get('version') is None
                    or not _check_hash(manifest['source'])):
                return False

            for key in manifest['binpkgs'][self.arch]:
                if not _check_hash(manifest['binpkgs'][self.arch][key]):
                    return False

            return True
        except (KeyError, IOError):
            return False


    def _suppress_old_packages(self, to_remove: set):
        """
        Suppress all the packages of the repository that have been replaced by
        new versions (see function _handle_binary_packages).

        Args:
            to_remove: set of the packages that MUST be removed
        """
        for filename in to_remove:
            file_path = os.path.join(self.curr_dir, filename)
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
        self.srcindex[manifest['source']['sha256']] = {
            'filename': manifest['source']['file'],
            'size': manifest['source']['size'],
            'version':manifest['version']}

        # Update binary-index
        for key in manifest['binpkgs'][self.arch]:
            pkg_filename = manifest['binpkgs'][self.arch][key]['file']
            pkg_path = os.path.join(self.curr_dir, pkg_filename)

            with tarfile.open(pkg_path, 'r:*') as mpk:
                stream = mpk.extractfile('./MMPACK/info').read()
                buf = yaml.safe_load(stream)
                buf[key].update({
                    'filename': pkg_filename,
                    'size': manifest['binpkgs'][self.arch][key]['size'],
                    'sha256': manifest['binpkgs'][self.arch][key]['sha256']
                    })

                self.binindex[key] = buf[key]

        srcindex_file = os.path.join(self.curr_dir, RELPATH_SOURCE_INDEX)
        yaml_serialize(self.srcindex, srcindex_file, True)
        binindex_file = os.path.join(self.curr_dir, RELPATH_BINARY_INDEX)
        yaml_serialize(self.binindex, binindex_file, True)


    def _upload_source_packages(self, manifest: dict):
        """
        Add the source package (file .tar.xz) in the repository. If a file
        already exists in the repository with the same name, it is replaced.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        pkg_filename = manifest['source']['file']

        source = os.path.join(RELPATH_WORKING_DIR, pkg_filename)
        destination = os.path.join(self.curr_dir, pkg_filename)
        shutil.move(source, destination)


    def _handle_binary_packages(self, count_src_refs: dict, manifest: dict,
                                to_remove: set):
        """
        Add the binary packages (files .mpk) in the repository. If some files
        already exist in the repository with same names, they are replaced.

        The binary packages that are not needed anymore (because they have been
        replaced by a new version) are placed in a set. This list permits to
        remember which packages MUST be removed after the whole upload is over.

        Each binary package possesses a pointer towards a source. When we remove
        a binary package, we decrement the number of references pointing to its
        source package. When the number of references of a source package
        reaches 0, we MUST remove this package after the whole upload is over.

        Args:
            count_src_refs: counter of references on the source packages.
            manifest: dictionary of the manifest file uploaded by the user.
            to_remove: set to fill with packages that MUST be removed after the
                       upload is over
        """
        for pkg_name in manifest['binpkgs'][self.arch]:
            pkg_filename = manifest['binpkgs'][self.arch][pkg_name]['file']

            if (pkg_name in self.binindex
                    and self.binindex[pkg_name]['filename'] != pkg_filename):
                to_remove.add(self.binindex[pkg_name]['filename'])
                srcsha256 = self.binindex[pkg_name]['srcsha256']
                count_src_refs[srcsha256] = count_src_refs.get(srcsha256) - 1
                if count_src_refs.get(srcsha256) == 0:
                    to_remove.add(self.srcindex[srcsha256]['filename'])

            source = os.path.join(RELPATH_WORKING_DIR, pkg_filename)
            destination = os.path.join(self.curr_dir, pkg_filename)
            shutil.move(source, destination)


    def _init_count_src_refs(self, count_src_refs: dict):
        """
        This function fills a dictionary where the keys correspond to hash of
        source packages and the values correspond to the number of binary
        packages referencing the key.

        Args:
            count_src_refs: dictionary to initialize.
        """
        for pkg_name in self.binindex:
            sha256 = self.binindex[pkg_name]['srcsha256']
            count_src_refs[sha256] = count_src_refs.get(sha256, 0) + 1


    def _upload(self, manifest: dict):
        """
        This function proceeds to the upload: upload the different packages
        (source and binary), update the indexes of the repository, and
        remove the packages not needed anymore.

        Args:
            manifest: dictionary of the manifest file uploaded by the user
        """
        # This dictionary permits to count the number of references on source
        # packages (how many binary packages refer to the source package). This
        # structure permits to suppress the source package that are not needed
        # anymore in the repository
        count_src_refs = dict()
        self._init_count_src_refs(count_src_refs)

        # Upload source package
        self._upload_source_packages(manifest)
        # Upload binary packages and remove the binary packages that are not
        # needed anymore
        to_remove = set()
        self._handle_binary_packages(count_src_refs, manifest, to_remove)

        # Update the binary-index and the source-index with the new packages
        self._update_indexes_repo(manifest)

        # Remove the files that are not needed anymore
        self._suppress_old_packages(to_remove)


    def _try_upload_files(self):
        manifest_file = os.path.join(RELPATH_WORKING_DIR, '*.mmpack-manifest')

        if len(glob.glob(manifest_file)) != 1:
            print('Abort: No manifest, or multiple manifests found')
            return

        manifest = yaml_load(glob.glob(manifest_file)[0])

        if not self._is_manifest_and_data_coherent(manifest):
            print('Abort: some fields missing in manifest or manifest is not ',
                  'coherent with data')
            return

        self._upload(manifest)
        print('Data proceeded successfully')


    def try_upload_archive(self, archive_name: str):
        """
        This function tries to upload the packages of the user.

        Args:
            archive_name: path through the archive containing the packages that
                          the user wants to upload.
        """
        try:
            print('Checking', archive_name)

            with tarfile.open(archive_name, 'r:*') as tar:
                tar.extractall(RELPATH_WORKING_DIR)

                os.remove(archive_name)

                self._try_upload_files()

        finally:
            pass
