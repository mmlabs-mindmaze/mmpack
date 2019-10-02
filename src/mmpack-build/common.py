# @mindmaze_header@
"""
A set of helpers used throughout the mmpack project
"""

import logging
import logging.handlers
import os
import sys

import platform

from hashlib import sha256
from subprocess import PIPE, run
from typing import Union

import yaml

CONFIG = {'debug': True, 'verbose': True}
LOGGER = None

# list of stored level-msg pairs of logged lines issued before the
# filename-based logger becomes available. Once it becomes available this
# list will populate the newly created log.
TMP_LOG_STRLIST = []


def set_log_file(filename):
    """
    Init logger to print to *filename*.
    Usually called via (e|i|d)print helpers.
    """
    global LOGGER  # pylint: disable=global-statement

    log_handler = logging.FileHandler(filename, mode='w')

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    LOGGER = logging.getLogger('mmpack-build')
    LOGGER.addHandler(log_handler)

    LOGGER.setLevel(logging.DEBUG)

    for level, line in TMP_LOG_STRLIST:
        LOGGER.log(level, line)


def _log_or_store(level, *args, **kwargs):
    """
    Log a message if the file-based logger has been created, otherwise keep
    the message along with the level in TMP_LOG_STRLIST for temporary
    storage until it because available
    """
    line = str(*args, **kwargs)
    if not LOGGER:
        TMP_LOG_STRLIST.append([level, line])
    else:
        LOGGER.log(level, line)


def log_info(*args, **kwargs):
    """
    write only to log as info
    """
    _log_or_store(logging.ERROR, *args, **kwargs)


def eprint(*args, **kwargs):
    """
    error print: print to stderr
    """
    _log_or_store(logging.ERROR, *args, **kwargs)
    print(*args, file=sys.stderr, **kwargs)


def wprint(*args, **kwargs):
    """
    warning print: print to stderr
    """
    _log_or_store(logging.WARNING, *args, **kwargs)
    print(*args, file=sys.stderr, **kwargs)


def iprint(*args, **kwargs):
    """
    info print: print only if verbose flag is set
    """
    _log_or_store(logging.INFO, *args, **kwargs)
    if CONFIG['verbose'] or CONFIG['debug']:
        print(*args, file=sys.stderr, **kwargs)


def dprint(*args, **kwargs):
    """
    debug print: standard print and log
    """
    _log_or_store(logging.DEBUG, *args, **kwargs)
    if CONFIG['debug']:
        print(*args, file=sys.stderr, **kwargs)


class ShellException(RuntimeError):
    """
    custom exception for shell command error
    """


def shell(cmd, log=True, input_str=None):
    """
    Wrapper for subprocess.run

    Args:
        cmd: Can be either a string or a list of strings
             shell() will pass the command through the shell if and only if
             the cmd argument is a string
        log: log command string on debug output if True
        input_str: string to send on standard input of the created process
    Raises:
        ValueError: if type of cmd is invalid
        ShellException: if the command run failed

    Returns:
        the output of the command decoded to utf-8
    """
    if isinstance(cmd, list):
        run_shell = False
        logmsg = ' '.join(cmd)
    elif isinstance(cmd, str):
        run_shell = True
        logmsg = cmd
    else:
        raise ValueError('Invalid shell argument type: ' + str(type(cmd)))

    if log:
        dprint('[shell] {0}'.format(logmsg))

    input_utf8 = input_str.encode('utf-8') if input_str else None

    try:
        ret = run(cmd, stdout=PIPE, shell=run_shell, input=input_utf8)
        if ret.returncode == 0:
            return ret.stdout.decode('utf-8')

        errmsg = 'Command "{:.50s}{:s}" failed with error {:d}' \
                 .format(logmsg, '...' if len(logmsg) > 50 else '',
                         ret.returncode)
        raise ShellException(errmsg)
    except FileNotFoundError:
        raise ShellException('failed to exec command')


# initialise a directory stack
PUSHSTACK = []


def pushdir(dirname):
    """
    Save and then change the current directory
    """
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
    """
    Get the git top directory
    """
    shell('git rev-parse --show-toplevel')


def remove_duplicates(lst):
    """
    remove duplicates from list (in place)
    """
    for elt in list(lst):
        while lst and lst.count(elt) > 1:
            lst.remove(elt)


def yaml_serialize(obj: Union[list, dict], filename: str,
                   use_block_style: bool = False) -> None:
    """
    Save object as yaml file of given name
    """
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
    """
    enforce yaml interpretation of given complex object type as unicode
    classes which want to benefit must add themselves as follows:

      yaml.add_representer(<class-name>, mm_representer)

    (Otherwise, they will be printed with a !!python/object tag)
    """
    return dumper.represent_data(repr(data))


def _set_representer(dumper, data):
    """
    dump python set as list
    """
    return dumper.represent_data(list(data))


yaml.add_representer(set, _set_representer)


def sha256sum(filename: str, follow_symlink: bool = True) -> str:
    """
    compute the SHA-256 hash of a file

    This returns the SHA256 string of filename. If follow_symlink is False,
    a symlink will not be followed. Additionnaly, a short indicator will be
    prefixed to the string returned ("reg-" for regular file, "sym-" for
    symlink) and the hash of a symlink will be the SHA256 of the target
    path of the link.

    Args:
        filename: path of file whose hash must be computed
        follow_symlink: symlink must not be followed and computed hash must
            be type specific

    Returns:
        a string containing hexadecimal value of hash
    """
    sha = sha256()
    if not follow_symlink and os.path.islink(filename):
        # Compute sha256 of symlink target and replace beginning with "sym"
        sha.update(os.readlink(filename).encode('utf-8'))
        shastr = sha.hexdigest()
        return "sym-" + shastr
    sha.update(open(filename, 'rb').read())
    hexdig = sha.hexdigest()

    if not follow_symlink:
        return "reg-" + hexdig

    return hexdig


def get_host_arch() -> str:
    """
    return the host arch
    """
    arch = platform.machine().lower()
    if arch == 'x86_64':
        arch = 'amd64'

    return arch


def get_host_dist() -> str:
    """
    return host distribution
    """
    try:
        for line in open("/etc/os-release", "r"):
            if line.startswith('ID='):
                return line[3:].strip()
    except FileNotFoundError:
        # if not linux, then windows
        pass
    return 'windows'


def get_host_arch_dist() -> str:
    """
    return the host arch

    Format is <cpu arch>-<os>, and the values are converted if necessary.
    possible archs: amd64, i386, aarch64, ...
    possible os: debian, windows, ...

    Eg.
        amd64-debian
        aarch64-debian
        i386-windows
    """

    return '{0}-{1}'.format(get_host_arch(), get_host_dist())


def parse_soname(soname: str) -> (str, str):
    """
    helper to parse soname
    www.debian.org/doc/debian-policy/ch-sharedlibs.html
    """
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


def shlib_keyname(soname: str) -> str:
    """
    Generate an identifier name from a soname that can be used across
    platform.

    example:
        libfoo-5.dll => libfoo5
        libfoo.so.5 => libfoo5
    """
    name, version = parse_soname(soname)
    return name + version  # libxxx.0.1.2 -> libxxx<ABI>


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    return yaml.load(open(filename, 'rb').read(), Loader=yaml.BaseLoader)


class Assert(AssertionError):
    """
    Wrapper over AssertionError which also logs the message as an error.
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _log_or_store(logging.ERROR, *args, **kwargs)
