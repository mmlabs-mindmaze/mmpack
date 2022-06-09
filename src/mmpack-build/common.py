# @mindmaze_header@
"""
A set of helpers used throughout the mmpack project
"""

import bz2
import logging
import logging.handlers
import lzma
import gzip
import os
import re
import shutil
import stat
import sys
import tarfile

import platform

from argparse import Action
from hashlib import sha256
from io import TextIOWrapper
from subprocess import PIPE, CalledProcessError, Popen, run
from typing import (Any, AnyStr, BinaryIO, Optional, Union, Dict, Tuple, List,
                    Set)

import urllib3
import yaml

from .errors import ShellException, DownloadError
from .yaml_dumper import MMPackDumper

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
    _log_or_store(logging.INFO, *args, **kwargs)


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


class _RWPopen(Popen):
    """
    Popen class with file like API
    """
    def write(self, buff: AnyStr) -> int:
        """
        same as Popen.stdout.write()
        """
        return self.stdin.write(buff)

    def read(self, size: int = -1) -> bytes:
        """
        same as Popen.stdout.write()
        """
        return self.stdout.read(size)


def shell(cmd, log: bool = True, input_str: str = None,
          log_stderr: bool = True, env: Dict[str, str] = None) -> str:
    """
    Wrapper for subprocess.run

    Args:
        cmd: Can be either a string or a list of strings
             shell() will pass the command through the shell if and only if
             the cmd argument is a string
        log: log command string on debug output if True
        input_str: string to send on standard input of the created process
        log_stderr: capture stderr and display with eprint() if True
        env: full environment with which the process must be executed. If None,
            the environment is inherited from the current process
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
                  stderr=PIPE if log_stderr else None, check=False, env=env)

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
    except FileNotFoundError as error:
        raise ShellException('failed to exec command') from error


def run_cmd(cmd: List[str], log: bool = True, env: Dict[str, str] = None,
            stdin: Optional[BinaryIO] = None):
    """Execute command.

    The called command is assumed to report failure reason (if applicable) on
    standard error or standard output.

    Args:
        cmd: list of argument forming cmd to run
        log: log command string on debug output if True
        env: full environment with which the process must be executed. If None,
            the environment is inherited from the current process
        stdin: Optional stream opened for reading in binary.

    raises:
        ShellException: the command return a failure code
    """
    if log:
        dprint('[run_cmd] {0}'.format(' '.join(cmd)))

    try:
        run(cmd, capture_output=True, encoding='utf-8',
            check=True, env=env, stdin=stdin)
    except CalledProcessError as err:
        if err.stdout or err.stderr:
            errmsg = err.stdout + err.stderr
        else:
            errmsg = str(err)

        raise ShellException(errmsg) from err


def run_build_script(name: str, execdir: str, specdir: str,
                     args: List[str] = None,
                     env: Optional[Dict[str, str]] = None):
    """
    Execute build script from spec dir

    Args:
        name: name of file script to execute
        execdir: path where the hook must be executed
        specdir: path where spec files should be found
        args: list of argument passed to script if not None
        env: environ variables to add in addition to the inherited env
    """
    # Run hook only if existing (try _hook suffix before giving up)
    script = os.path.join(specdir, name + '_script')
    if not os.path.exists(script):
        script = os.path.join(specdir, name + '_hook')
        if not os.path.exists(script):
            return

    hook_env = os.environ.copy()
    hook_env.update(env if env else {})

    cmd = ['sh', os.path.abspath(script)]
    cmd += args if args else []

    pushdir(os.path.abspath(execdir))
    run_cmd(cmd, env=hook_env)
    popdir()


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

    with open_compressed_file(filename, 'w',
                              newline='\n', encoding='utf-8') as outfile:
        yaml.dump(obj, outfile,
                  default_flow_style=default_flow_style,
                  default_style='',
                  allow_unicode=True,
                  indent=4,
                  Dumper=MMPackDumper)
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


_DIST_DERIVATIVES = {
    'debian': {'debian', 'ubuntu', 'linuxmint', 'raspbian'},
}


def get_host_dist() -> str:
    """
    return host distribution
    """
    try:
        for line in open("/etc/os-release", "r"):
            if line.startswith('ID='):
                dist = line[3:].strip()
                break
    except FileNotFoundError:
        # if not linux, then windows
        dist = 'windows'

    # Remap distributation derivative
    for orig_dist, derivatives in _DIST_DERIVATIVES.items():
        if dist in derivatives:
            dist = orig_dist
            break

    return dist


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

    if soname.endswith('.so'):
        return (soname[:-len('.so')], '')
    elif not soname.endswith('.dll'):
        raise ValueError('failed to parse SONAME: ' + soname)

    # Assume format: <name>[-<version>].dll
    split = soname[:-len('.dll')].rsplit('-', 1)
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
    if name[-1].isdigit() and version:
        name += '-'

    name = name.lower().replace('_', '-')

    # Sometime on some platform, mostly on windows, shared libraries names are
    # not prefixed with "lib". This is an issue because often, those project on
    # other platform, like Linux, they do add the lib prefix in the shared lib
    # name. Consequently, to keep cross-platform consistent keyname, we prefix
    # lib if it does not exist. This lib prefix also avoids name clash between
    # packages holding executable named after the same root as the shared
    # library.
    if not name.startswith('lib'):
        name = 'lib' + name

    return name + version  # libxxx.0.1.2 -> libxxx<ABI>


def yaml_load(filename: str):
    """
    helper: load yaml file with BasicLoader
    """
    with open_compressed_file(filename, 'rb') as fileobj:
        return yaml.load(fileobj.read(), Loader=yaml.BaseLoader)


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
    with open_compressed_file(dstfile, 'wb', compression) as fileobj:
        tar = tarfile.open(fileobj=fileobj, mode='w|')
        tar.add(srcdir, recursive=True, filter=_reset_entry_attrs, arcname='.')
        tar.close()


def _compression_from_filename(path: str) -> str:
    # Guess compression from extension
    compression = path.rsplit('.', 1)[-1].lower()
    if compression not in ('gz', 'xz', 'bz2', 'zst'):
        compression = ''

    return compression


def _compression_from_magic_number(path: str) -> str:
    try:
        with open(path, 'rb', buffering=0) as fileobj:
            magic = fileobj.read(6)
    except FileNotFoundError:
        return _compression_from_filename(path)

    if not magic:
        compression = _compression_from_filename(path)
    elif magic[:2] == b'\x1f\x8b':
        compression = 'gz'
    elif magic[:6] == b'\xfd\x37\x7a\x58\x5a\x00':
        compression = 'xz'
    elif magic[:3] == b'\x42\x5a\x68':
        compression = 'bz2'
    elif magic[:4] == b'\x28\xb5\x2f\xfd':
        compression = 'zst'
    else:
        compression = ''

    return compression


def open_compressed_file(path: str, mode: str = 'rt',
                         compression: Optional[str] = None, **kwargs):
    """
    Open a compressed or uncompressed file transparently. The selection of
    compression is done based on path extension unless specified in arguments

    Args:
        path: file to open
        mode:  optional string that specifies the mode in which the file is
            opened. It has the same meaning as in open(). defaults to 'rt' if
            unspecified.
            compression: option string that specifies the type of compression.
                Supports 'gz', 'xz', 'bz2', 'ztd' and ''. If unspecified it is
                guessed from file magic number or filename.
            **kwargs: keyword arguments passed to the opening function.

    Return:
        A file object.

    Raises:
        ValueError: compression is not a supported string.
    """
    if compression is None:
        if 'w' not in mode:
            compression = _compression_from_magic_number(path)
        else:
            compression = _compression_from_filename(path)

    if compression == 'gz':
        fileobj = gzip.GzipFile(path, mode.replace('t', ''), mtime=0)
        if 'b' not in mode:
            fileobj = TextIOWrapper(fileobj, **kwargs)
        return fileobj
    elif compression == 'xz':
        return lzma.open(path, mode, **kwargs)
    elif compression == 'bz2':
        return bz2.open(path, mode, **kwargs)
    elif compression == 'zst':
        if 'w' in mode:
            return _RWPopen(['zstd', '-9fqo', path], stdin=PIPE)
        else:
            return _RWPopen(['zstd', '-dqc', path], stdout=PIPE)
    elif compression == '':
        return open(path, mode, **kwargs)

    # Unsupported compression if we reach here
    raise ValueError(f'Invalid compression "{compression}" when'
                     f'opening compressed file {path}')


def specs_load(specfile: str) -> Dict[str, Any]:
    """
    Load specs for file and return the dictionary out of it. Upgrade the format
    if old specfile.
    """
    specs = yaml_load(specfile)

    # If old specfile format, reorganize it to fit the new format
    if 'general' in specs:
        new_specs = specs['general']
        pkgs = {n: cfg for n, cfg in specs.items() if n != 'general'}
        new_specs['custom-pkgs'] = pkgs
        specs = new_specs

    return specs


def get_name_version_from_srcdir(srcdir: str) -> Tuple[str, str]:
    """
    Read unpacked source dir and get name and version of a source package
    """
    specs = specs_load(srcdir + '/mmpack/specs')
    return (specs['name'], specs['version'])


def get_http_req(url: str) -> urllib3.response.HTTPResponse:
    """
    Get urllib3 http response to request of remote resource
    """
    request = urllib3.PoolManager().request('GET', url)
    if request.status != 200:
        eprint('Failed ' + request.reason)
        raise DownloadError(request.reason, url)

    return request


def download(url: str, path: str):
    """
    Download file from url to the specified path
    """
    iprint('Downloading {} ... '.format(url))

    request = get_http_req(url)
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


def find_license(directory: str = '.') -> str:
    """
    guess project license from a license file (case insensitive)
    Assuming a single license file at the top of the tree

    Returns:
        The license file name on success, None otherwise
    """
    is_license = re.compile(r'(LICENSE|COPYING)', re.IGNORECASE)
    try:
        for entry in os.listdir(directory):
            filename = os.path.join(directory, entry)
            if os.path.isfile(filename) and is_license.match(entry):
                return entry
        return None
    except Exception:  # pylint: disable=broad-except
        return None


_TRUE_STRINGS = {'true', 't', 'yes', 'y', 'on', '1'}
_FALSE_STRINGS = {'false', 'f', 'no', 'n', 'off', '0'}


def str2bool(value: str) -> bool:
    """
    Convert a string to bool value

    Raise:
        TypeError: value is not a string
        ValueError: the string value does not represent a boolean
    """
    lowerval = value.strip().lower()
    if lowerval in _TRUE_STRINGS:
        return True
    if lowerval in _FALSE_STRINGS:
        return False

    raise ValueError('"{}" does not represent a boolean value'.format(value))


def extract_matching_set(pcre: str, str_set: Set[str]) -> Set[str]:
    """
    given some pcre, get the set of matching string from str_set.
    Those matching are removed from this set given in argument

    Args:
        pcre: regex to test
        str_set: set of string to update

    Return:
        The set of string in str_set matching pcre argument

    """
    matching_re = re.compile(pcre)
    matching_set = {f for f in str_set if matching_re.fullmatch(f)}
    str_set.difference_update(matching_set)
    return matching_set


# pylint: disable=unused-argument
def _onerror_handler(func, path, exc_info):
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWRITE)
        try:
            os.remove(path)
        except IsADirectoryError:
            os.rmdir(path)


def rmtree_force(path: str):
    """
    Call to shutil.rmtree() that tries to delete file even they are read-only.
    On POSIX, a file can always be removed if the calling process has the write
    access on containing folder. On Windows the write permission must be added
    as well.
    """
    shutil.rmtree(path, onerror=_onerror_handler)


def wrap_str(text: str,
             maxlen: int = 80,
             indent: str = '',
             split_token: str = ' ') -> List[str]:
    """
    Wrap a text string

    Args:
        text: the string to wrap
        maxlen: the maximum line allowed before wrapping
        indent: string to insert before each new line after split
        split_token: the token after each a line may be split

    Return:
        the wrapped text
    """
    lines = []

    while len(text) > maxlen:
        prefix = text[:maxlen].rsplit(split_token, 1)[0]
        prefix += split_token
        text = text[len(prefix):]
        lines.append(prefix)

    lines.append(text)

    return ('\n' + indent).join(lines)


class DeprecatedStoreAction(Action):
    """Action that warns as deprecated option when used"""
    def __call__(self, parser, namespace, values, option_string=None):
        wprint(f'Option {option_string} of {parser.prog} is deprecated')
        setattr(namespace, self.dest, values)
