#!/usr/bin/python

import sys
import yaml
import os
from mmpack_build.common import sha256sum

def is_main_key_well_formed(data: dict, main_key: str) -> bool:
    return ((main_key in data) and ('file' in data[main_key])
            and ('sha256' in data[main_key]) and ('size' in data[main_key]))


def is_manifest_well_formed(data: dict) -> bool:
    if not is_main_key_well_formed(data, 'source'):
        return False

    if not 'binpkgs' in data:
        return False
    
    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            if not is_main_key_well_formed(data['binpkgs'][arch], key):
                return False

    return True


def is_main_key_and_data_coherent(data:dict, main_key: str) -> bool:
    return ((os.path.isfile(data[main_key]['file']))
            and 
            (os.path.getsize(data[main_key]['file']) == data[main_key]['size'])
            and (sha256sum(data[main_key]['file']) == data[main_key]['sha256']))


def is_manifest_and_data_coherent(data: dict) -> bool:
    if not is_main_key_and_data_coherent(data, 'source'):
        return False

    for arch in data['binpkgs']:
        for key in data['binpkgs'][arch]:
            if not is_main_key_and_data_coherent(data['binpkgs'][arch], key):
                return False

    return True

def check_manifest_and_data(filename: str) -> bool:
    with open(sys.argv[1], 'r') as file:
        data = yaml.safe_load(file)

        # check that the sources are correct and then check that the data in
        # manifest corresponds to actual files provided by the user
        return (is_manifest_well_formed(data) 
                and is_manifest_and_data_coherent(data))
