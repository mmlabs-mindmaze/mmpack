#!/usr/bin/env python3
'''
TODOC
'''

import os
import sys
from subprocess import PIPE, run
from typing import Union
import syslog
import yaml

CONFIG = {'debug': True, 'verbose': True}


def eprint(message):
    'error print: print to stderr'
    message = 'ERR: ' + message
    syslog.syslog(syslog.LOG_ERR, message)
    print(message, file=sys.stderr)


def iprint(message):
    'info print: print only if verbose flag is set'
    message = 'INFO: ' + message
    syslog.syslog(syslog.LOG_INFO, message)
    if CONFIG['verbose'] or CONFIG['debug']:
        print(message, file=sys.stderr)


def dprint(message):
    'debug print: standard print and log'
    message = 'DEBUG: ' + message
    syslog.syslog(syslog.LOG_DEBUG, message)
    if CONFIG['debug']:
        print(message, file=sys.stderr)


class ShellException(RuntimeError):
    'custom exception for shell command error'


def shell(cmd):
    'Wrapper for subprocess.run'
    dprint('[shell] {0}'.format(cmd))
    ret = run(cmd, stdout=PIPE, shell=True)
    if ret.returncode == 0:
        return ret.stdout.decode('utf-8')
    else:
        raise ShellException('failed to exec command')


# initialise a directory stack
PUSHSTACK = []


def pushdir(dirname):
    'Save and then change the current directory'
    PUSHSTACK.append(os.getcwd())
    os.chdir(dirname)


def popdir(num=1):
    'Remove the top n entries from the directory stack'
    os.chdir(PUSHSTACK.pop())
    if num > 1:
        popdir(num - 1)


def git_root():
    'Get the git top directory'
    shell('git rev-parse --show-toplevel')


def remove_duplicates(lst):
    'remove duplicates from list (in place)'
    for elt in lst:
        while lst and lst.count(elt) > 1:
            lst.remove(elt)


def filetype(filename):
    'get file type'
    file_type = shell('file  --brief --preserve-date {}'.format(filename))
    # try to read file type first
    if file_type.startswith('ELF '):
        return 'elf'
    elif file_type.startswith('pe'):
        return 'pe'

    # return file extension otherwise
    # eg. python, c-header, ...
    return os.path.splitext(filename)[1][1:].strip().lower()


def yaml_serialize(obj: Union[list, dict], filename: str) -> None:
    'Save object as yaml file of given name'
    with open(filename, 'w+') as outfile:
        yaml.dump(obj, outfile,
                  default_flow_style=False,
                  allow_unicode=True,
                  indent=4)
    dprint('wrote {0}'.format(filename))


def mm_representer(dumper, data):
    '''
    enforce yaml interpretation of given complex object type as unicode
    classes which want to benefit must add themselves as follow:
      yaml.add_representer(<class-name>, mm_representer)

    (Otherwise, they will be printed with a !!python/object tag)
    '''
    return dumper.represent_data(repr(data))
