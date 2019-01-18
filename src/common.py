# @mindmaze_header@
'''
A set of helpers used throughout the mmpack project
'''

import logging
import logging.handlers
import os
import sys
from subprocess import PIPE, run
from typing import Union, Dict
from hashlib import sha256
import platform
import yaml
from decorators import run_once
from xdg import XDG_DATA_HOME

CONFIG = {'debug': True, 'verbose': True}


@run_once
def _init_logger_if_not():
    '''
    Init logger to print to *log_file*.
    Usually called via (e|i|d)print helpers.

    If verbose flag is set, it will log up to DEBUG level
    otherwise, only up to INFO level.
    '''
    log_file = XDG_DATA_HOME + '/mmpack.log'
    log_handler = logging.handlers.TimedRotatingFileHandler(log_file)
    logger = logging.getLogger()
    logger.addHandler(log_handler)

    if CONFIG['debug']:
        logger.setLevel(logging.DEBUG)
    elif CONFIG['debug'] or CONFIG['verbose']:
        logger.setLevel(logging.INFO)
    else:
        logger.setLevel(logging.WARNING)


def eprint(*args, **kwargs):
    'error print: print to stderr'
    _init_logger_if_not()
    logging.error(*args, **kwargs)
    print(*args, file=sys.stderr, **kwargs)


def iprint(*args, **kwargs):
    'info print: print only if verbose flag is set'
    _init_logger_if_not()
    logging.info(*args, **kwargs)
    if CONFIG['verbose'] or CONFIG['debug']:
        print(*args, file=sys.stderr, **kwargs)


def dprint(*args, **kwargs):
    'debug print: standard print and log'
    _init_logger_if_not()
    logging.debug(*args, **kwargs)
    if CONFIG['debug']:
        print(*args, file=sys.stderr, **kwargs)


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
    dprint('cd ' + dirname)
    os.chdir(dirname)


def popdir(num=1):
    'Remove the top n entries from the directory stack'
    lastdir = PUSHSTACK.pop()
    dprint('cd ' + lastdir)
    os.chdir(lastdir)
    if num > 1:
        popdir(num - 1)


def git_root():
    'Get the git top directory'
    shell('git rev-parse --show-toplevel')


def remove_duplicates(lst):
    'remove duplicates from list (in place)'
    for elt in list(lst):
        while lst and lst.count(elt) > 1:
            lst.remove(elt)


def yaml_serialize(obj: Union[list, dict], filename: str,
                   use_block_style: bool=False) -> None:
    'Save object as yaml file of given name'
    default_flow_style = None
    if use_block_style:
        default_flow_style = False

    with open(filename, 'w+', newline='\n') as outfile:
        yaml.dump(obj, outfile,
                  default_flow_style=default_flow_style,
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


def _set_representer(dumper, data):
    'dump python set as list'
    return dumper.represent_data(list(data))


yaml.add_representer(set, _set_representer)


def sha256sum(filename: str) -> str:
    ''' compute the SHA-256 hash a file

    If file is a symlink, the hash string will be the hash of the target path
    with "sym" replacing the 3 first characters of SHA256 string

    Args:
        filename: path of file whose hash must be computed
    Returns:
        a string containing hexadecimal value of hash
    '''
    sha = sha256()
    if os.path.islink(filename):
        # Compute sha256 of symlink target and replace beginning with "sym"
        sha.update(os.readlink(filename).encode('utf-8'))
        shastr = sha.hexdigest()
        return "sym" + shastr[3:]
    sha.update(open(filename, 'rb').read())
    return sha.hexdigest()


def get_host_arch() -> str:
    'return the host arch'
    arch = platform.machine().lower()
    if arch == 'x86_64':
        arch = 'amd64'

    return arch


def get_host_dist() -> str:
    'return host distribution'
    try:
        for line in open("/etc/os-release", "r"):
            if line.startswith('ID='):
                return line[3:].strip()
    except FileNotFoundError:
        # if not linux, then windows
        return 'windows'


def get_host_arch_dist() -> str:
    ''' return the host arch

    Format is <cpu arch>-<os>, and the values are converted if necessary.
    possible archs: amd64, i386, aarch64, ...
    possible os: debian, windows, ...
    Eg.
        amd64-debian
        aarch64-debian
        i386-windows
    '''

    return '{0}-{1}'.format(get_host_arch(), get_host_dist())


def parse_soname(soname: str) -> (str, str):
    ''''helper to parse soname
    www.debian.org/doc/debian-policy/ch-sharedlibs.html
    '''
    # try format: <name>.so.<major-version>
    try:
        name, major = soname.split('.so.')
        return (name, major)
    except ValueError:
        pass

    # try format: <name>-<version>.[so|dll]
    # return the full version (not only the major part)
    if soname.endswith('.so'):
        soname = soname[:-len('.so')]
    elif soname.endswith('.dll'):
        soname = soname[:-len('.dll')]
    else:
        raise ValueError('failed to parse SONAME: ' + soname)

    split = soname.split('-')
    if len(split) == 1:
        version = '0'
    else:
        version = split[-1]
    name = '-'.join(split[0:-1])
    version = split[-1]

    return (name, version)


def yaml_load(filename: str):
    'helper: load yaml file with BasicLoader'
    return yaml.load(open(filename, 'rb').read(), Loader=yaml.BaseLoader)


def env_path_prepend(env: Dict[str, str], varname: str, value: str):
    ''' prepend value to the varname key ov given environment

    varname does not need to already be present within the env
    '''
    previous = env.get(varname, None)
    env[varname] = value
    if previous:
        env[varname] += os.pathsep + previous
