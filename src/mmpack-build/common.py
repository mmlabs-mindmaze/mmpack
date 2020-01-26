# @mindmaze_header@
"""
A set of helpers used throughout the mmpack project
"""

import logging
import logging.handlers
import os
import re
import sys
import tarfile

import platform

from hashlib import sha256
from subprocess import PIPE, run
from typing import Union, Tuple, List

import urllib3
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


def shell(cmd, log: bool = True, input_str: str = None,
          log_stderr: bool = True) -> str:
    """
    Wrapper for subprocess.run

    Args:
        cmd: Can be either a string or a list of strings
             shell() will pass the command through the shell if and only if
             the cmd argument is a string
        log: log command string on debug output if True
        input_str: string to send on standard input of the created process
        log_stderr: capture stderr and display with eprint() if True
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
        ret = run(cmd, stdout=PIPE, shell=run_shell, input=input_utf8,
                  stderr=PIPE if log_stderr else None)

        # Reproduce stderr of command with eprint() if requested
        if log_stderr:
            for line in ret.stderr.decode('utf-8').splitlines():
                eprint(line)

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
        name, major = soname.rsplit('.so.', 1)
        return (name, major)
    except ValueError:
        pass

    if soname.endswith('.so'):
        return (soname[:-len('.so')], '')

    if soname.endswith('.dll'):
        soname = soname[:-len('.dll')]
    else:
        raise ValueError('failed to parse SONAME: ' + soname)

    # Assume format: <name>[-<version>].dll
    split = soname.rsplit('-', 1)
    name = split[0]
    if len(split) == 1:
        version = ''
    else:
        version = split[1]

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

    # Allow to distinguish libfoo1.so.0 from libfoo.so.10
    if name[-1].isdigit() and version != '':
        name += '-'

    name = name.lower()
    if not name.startswith('lib'):
        name = 'lib' + name

    return name + version  # libxxx.0.1.2 -> libxxx<ABI>


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    return yaml.load(open(filename, 'rb').read(), Loader=yaml.BaseLoader)


def convert_path_native(path: str) -> str:
    """
    helper: permits to convert a path in the way the natif system would do
    """

    if get_host_dist() == 'windows':
        return shell(['cygpath', '-w', path], log=False).strip()
    else:
        return path


def concatenate_unix(head_path: str = "", tail_path: str = "") -> str:
    """
    helper: permits to concatenate two paths to form a correct one
    (specially on windows, mingwin automatically writes C:/msys64 at
    the beginning of every path, so a simple concatenation would give
    C:/msys64head_pathC:/msys64tail_path)
    """

    if get_host_dist() == 'windows':
        if head_path:
            head_path = shell(['cygpath', '-u', head_path], log=False).strip()
        if tail_path:
            tail_path = shell(['cygpath', '-u', tail_path], log=False).strip()

    if head_path and tail_path and tail_path.startswith('/'):
        tail_path = tail_path[1:]

    return os.path.join(head_path, tail_path)


class Assert(AssertionError):
    """
    Wrapper over AssertionError which also logs the message as an error.
    """
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _log_or_store(logging.ERROR, *args, **kwargs)


def _reset_entry_attrs(tarinfo: tarfile.TarInfo):
    """
    filter function for tar creation that will remove all file attributes
    (uid, gid, mtime) from the file added to tar would can make the build
    of package not reproducible.

    Args:
        tarinfo: entry being added to the tar

    Returns:
        the modified tarinfo that will be actually added to tar
    """
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = 'root'
    tarinfo.mtime = 0

    if tarinfo.name.lower().endswith('.dll'):
        tarinfo.mode = 0o755

    return tarinfo


def create_tarball(srcdir: str, dstfile: str, compression: str = '') -> None:
    """
    Generate a tarball from the content of a folder. The generated file should
    be for deterministic build. Hence all user, group member ship, mode
    (excepting for the execution but), timestamps will be set to generic
    values.

    Args:
        srcfolder: folder whose content will be put in the tarball
        dstfile: path of the generated tarball
        compression: compression algorithm to used with the tarball. It must be
            one of the following string:
            - '': create a tarfile without compression (default)
            - 'gz': create a tarfile with gzip compression
            - 'bz2': create a tarfile with bzip2 compression
            - 'xz': create a tarfile with lzma compression
    """
    tar = tarfile.open(dstfile, 'w:' + compression)
    tar.add(srcdir, recursive=True, filter=_reset_entry_attrs, arcname='.')
    tar.close()


def get_name_version_from_srcdir(srcdir: str) -> Tuple[str, str]:
    """
    Read unpacked source dir and get name and version of a source package
    """
    specs = yaml_load(srcdir + '/mmpack/specs')
    return (specs['general']['name'], specs['general']['version'])


def download(url: str, path: str):
    """
    Download file from url to the specified path
    """
    iprint('Downloading {} ... '.format(url))

    request = urllib3.PoolManager().request('GET', url)
    if request.status != 200:
        eprint('Failed ' + request.reason)
        raise RuntimeError('Failed to download {}: {}'
                           .format(url, request.reason))

    with open(path, 'wb') as outfile:
        outfile.write(request.data)

    iprint('Done')


def list_files(topdir: str, exclude_dirs: bool = False) -> List[str]:
    """
    List files in topdir recursively. This does not follow symbolic links.

    Args:
        topdir: folder path whose content will be listed
        exclude_dirs: if True, directory element will not be listed

    Return:
        sorted list of files relative to topdir. The path separator of the
        listed elements will always be forward slash '/', no matter the
        platform (like glob does).
    """
    filelist = []

    for root, dirs, files in os.walk(topdir):
        reldir = os.path.relpath(root, topdir)
        reldir = '' if reldir == '.' else reldir + '/'
        filelist.extend([reldir + f for f in files])
        if not exclude_dirs:
            filelist.extend([reldir + d for d in dirs])

    return sorted(filelist)


def find_license(directory: str = None) -> str:
    """
    guess project license from a license file (case insensitive)
    Assuming a single license file at the top of the tree

    Returns:
        The license file name on success, None otherwise
    """
    is_license = re.compile(r'(LICENSE|COPYING)', re.IGNORECASE)
    try:
        for entry in os.listdir(directory):
            if os.path.isfile(entry) and is_license.match(entry):
                return entry
        return None
    except Exception:  # pylint: disable=broad-except
        return None
