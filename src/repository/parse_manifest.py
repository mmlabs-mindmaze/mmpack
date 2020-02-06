#!/usr/bin/python
"""
Module provinding helpers.
- It provides a helper to check that all the information required in a manifest
are present.
- It provides a helper to check that the data provided by a user are coherent
with the information given in the manifest.
- It provides a helper to replace the files upload with the old files (if any)
"""

import os
import shutil
import yaml
from mmpack_build.common import sha256sum
import mmpack_build.src_package as sp

def _is_main_key_well_formed(data: dict, main_key: str) -> bool:
    """
    check that in a dictionnary, a given key posseses 3 specific key/value pairs

    Args:
        data: the dictionnary
        main_key: the key we are checking whether it is well-formed or not
    """
    return ((main_key in data) and ('file' in data[main_key])
            and ('sha256' in data[main_key]) and ('size' in data[main_key]))


def _is_manifest_well_formed(data: dict) -> bool:
    """
    check that in a dictionnary there is a key source and a key binpkgs, and
    that each subkey is well-formed (possesses 3 specific key/value pairs)

    Args:
        data: the dictionnary to check
    """
    if not 'source' in data or not 'name' in data or not 'binpkgs' in data:
        return False

    if not _is_main_key_well_formed(data, 'source'):
        return False

    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            if not _is_main_key_well_formed(data['binpkgs'][arch], key):
                return False

    return True


def _is_main_key_and_data_coherent(data: dict, main_key: str, prefix: str) -> bool:
    """
    Check that the information provided in a dictionnary by a given key are
    true (the files information given by the key are effectively true: there
    exists a file with the same information in the prefix)

    Args:
        data: the dictionnary
        main_key: the key from which the information are checked
        prefix: prefix in which the files are looked for
    """
    filename = os.path.join(prefix, data[main_key]['file'])

    return ((os.path.isfile(filename))
            and
            (os.path.getsize(filename) == data[main_key]['size'])
            and (sha256sum(filename) == data[main_key]['sha256']))


def _is_manifest_and_data_coherent(data: dict, prefix: str) -> bool:
    """
    Check that the information provided in a dictionnary are coherent: there
    exist actual files in prefix having the characteristic provided by the keys
    of the dictionnary.

    Args:
        data: the dictionnary
        prefix: prefix in which the files are looked for
    """
    if not _is_main_key_and_data_coherent(data, 'source', prefix):
        return False

    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            if not _is_main_key_and_data_coherent(data['binpkgs'][arch], key,
                                                  prefix):
                return False

    return True


def _check_manifest_and_data(data: dict, prefix: str) -> bool:
    """
    Given a dictionnary, check that it is coherent (it possesses the good keys,
    and each key possesses 3 specific Key/value pairs), and check that the data
    provided by the user are coherent with the information written in the
    dictionnary (there exist files in the prefix whose caracteristic correspond
    to the one specified in the dictionnary)

    Args:
        data: the dictionnary to check
        prefix: prefix in which the files are looked for
    """
    # check that the sources are correct and then check that the data in
    # manifest corresponds to actual files provided by the user
    return (_is_manifest_well_formed(data)
            and _is_manifest_and_data_coherent(data, prefix))


def _add_keys_in_dict(data: dict, data_binary_ind: dict, dest: str):
    """
    Add the keys/values pairs of the packages added to the database. And then
    write an updated version of the binary_index file on the server.

    Args:
        data: dictionnary of the uploaded packages by the user
        data_binary_ind: dictionnary of the packages present on the database
        dest: directory where to write the new binary_index file
    """
    # read the mpk and add new keys/values to the dictionnary of the packages
    # present in the database
    
    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            archive_name = data['binpkgs'][arch][key]['filename']
            with tarfile.open(dest + archive_name, 'r:*') as archive:
                buf = archive.extractfile('./MMPACK/info').read()

                #TODO: create an entry for the dictionnary





    os.remove(dest + 'binary_index')
    binary_ind = os.open(dest + 'binary_index', 'w+')
    os.write(str(data_binary_ind))



def _replace_files(data: dict, prefix: str, data_binary_ind: dict,
                   dest: str):
    """
    Replace the files in the package database (if any) by the one uploaded by
    the user

    Args:
        data: dictionnary of the file uploaded by the user
        prefix: directory where the file uploaded are temporaly stored
        data_binary_ind: dictionnary of the package present in the database
        dest: directory where the packages are stored
    """
    if data['name'] in data_binary_ind:
        os.remove(dest + data_binary_ind[data['name']]['filename'])
        del data_binary_ind[data['name']]
    
    uploaded = os.path.join(prefix, data['source']['file'])
    shutil.move(uploaded, dest + data['source']['file'])

    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            if key in data_binary_ind:
                os.remove(dest + data_binary_ind['binpkgs'][arch][key]['filename'])
                del data_binary_ind['binpkgs'][arch][key]
                
            uploaded = os.path.join(prefix, data['binpkgs'][arch][key]['filename'])
            shutil.move(uploaded, dest + data['binpkgs'][arch][key]['filename'])

    _add_keys_in_dict(data, data_binary_ind, dest)


def try_upload(manifest: str, prefix: str, binary_ind: str, dest: str) -> bool:
    """
    Check that the manifest and the data provided by the user are coherent
    (the manifest contains all the required keys, and the user have provided
    files that correspond to the information written in the manifest). In case
    the verification is successfull then the files of the user are uploaded.

    Args:
        manifest: path through the manifest
        prefix: path through the server has uploaded the files of the user
        binary_ind: path through the binary index
        dest: path through the directory where the packages are sotred
    """
    with open(manifest, 'r') as manifest_file:
        data = yaml.safe_load(manifest_file)

        if not _check_manifest_and_data(data, prefix):
            return False

        with open(binary_ind, 'r') as binary_index:
            data_binary_ind = yaml.safe_load(binary_index)
            _replace_files(data, prefix, data_binary_ind, dest)
            sp.generate_binary_packages()

        return True
