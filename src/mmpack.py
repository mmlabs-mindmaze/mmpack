#!/usr/bin/env python3
'''
This is a stub for all mmpack commands

Usage:
mmpack <command> [command-options]

List all mmpack commands:
mmpack --help

More infos about a specific command:
mmpack <command> --help

TODO:
- user commands to implement
  * config
  * download
  * info/show
  * install
  * reinstall
  * search
  * uninstall
  * update/make-cache
  * upgrade

- package creation commands to implement:
  * pkg-check
  * pkg-create
  * pkg-show

- package DB commands to implement:
  * provides
  * ...


'''

import sys
import glob
import re
import subprocess

from os import path


def cmdlist():
    'get mmpack subcommand list'
    cmds = glob.glob('{0}/mmpack-*'
                     .format(path.join(path.dirname(__file__))))
    names = [re.split(".*/mmpack-([a-z-]*).*", cmd)[1] for cmd in cmds]
    names.sort()
    return names


def usage():
    'show usage'
    print(__doc__)
    print('full cmd list:')
    for cmd in cmdlist():
        print('\t * {0}'.format(cmd))


def main():
    '''try to run the following command:
       mmpack cmd [options] -> ./mmpack-{cmd}.py [options]
    '''
    try:
        name = '{0}/mmpack-{1}.py'.format(path.join(path.dirname(__file__)),
                                          sys.argv[1])
        cmd = [name] + sys.argv[2:]
        return subprocess.call(cmd)
    except (IndexError, FileNotFoundError):
        return usage()


if __name__ == '__main__':
    main()
