#!/usr/bin/env python3

from subprocess import PIPE, run
import sys

class ShellException(RuntimeError):
    'custom exception for shell command error'


def shell(cmd):
    'Wrapper for subprocess.run'
    ret = run(cmd, stdout=PIPE, shell=True)
    if ret.returncode == 0:
        return ret.stdout.decode('utf-8')
    else:
        raise ShellException('failed to exec command')


if __name__ == '__main__':
    print(shell(' '.join(sys.argv[1:])))
