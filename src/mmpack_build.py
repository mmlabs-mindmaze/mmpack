# @mindmaze_header@
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
    cmds = glob.glob('{0}/mmpack_*.py'
                     .format(path.join(path.dirname(__file__))))
    names = []
    for cmd in cmds:
        names.append(re.split(".*/mmpack_(.*).py", cmd)[1].replace('_', '-'))
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
        cmd = sys.argv[1].replace('-', '_')
        name = '{0}/mmpack_{1}.py'.format(path.join(path.dirname(__file__)),
                                          cmd)
        cmd = [name] + sys.argv[2:]
        return subprocess.call(cmd)
    except (IndexError, FileNotFoundError):
        return usage()


if __name__ == '__main__':
    main()
