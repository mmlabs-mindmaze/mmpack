# @mindmaze_header@
"""
Create a mmpack package

Usage:

mmpack pkg-create [--git|--src|--mmpack-src|--path]
                  [--tag <tag>] [--prefix <prefix>] [--skip-build-tests]
                  [path_or_url]

Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.

The type of argument is guessed and the appropriate create method will be used.
However the method used can be forced using the flags:
 '--git': assumes a git url or path to a local git repo
 '--src': assumes a source tarball
 '--path': assumes path to a mmpack packaging
 '--mmpack-src': assumes argument is a mmpack source tarball

In case of git method, if tag is specified, it will be used to build the commit
referenced by it. Additionally, for all method, it will indicate the name of
sub folder in the cache to use to build the packages.

If a prefix is given, work within it instead.

Examples:
# From any subfolder of the project
$ mmpack pkg-create

# From anywhere
$ mmpack pkg-create --tag v1.0.0-custom-tag-target \
ssh://git@intranet.mindmaze.ch:7999/~user/XXX.git
"""

import os
import tarfile

from argparse import ArgumentParser, RawDescriptionHelpFormatter
from subprocess import call, DEVNULL

from . common import set_log_file
from . src_package import SrcPackage
from . workspace import Workspace, find_project_root_folder
from . source_tarball import SourceTarball


CMD = 'pkg-create'


def parse_options(argv):
    """
    parse and check options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--git', dest='method',
                       action='store_const', const='git',
                       help='interpret argument as url/path to git repository')
    group.add_argument('--path', dest='method',
                       action='store_const', const='path',
                       help='interpret argument as path to folder with '
                            'a mmpack dir containing specs')
    group.add_argument('--src', dest='method',
                       action='store_const', const='tar',
                       help='interpret argument as source tarball')
    group.add_argument('--mmpack-src', dest='method',
                       action='store_const', const='srcpkg',
                       help='interpret argument as mmpack source tarball')
    parser.add_argument('-t', '--tag',
                        action='store', dest='tag', type=str,
                        help='project tag')
    parser.add_argument('-p', '--prefix',
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    parser.add_argument('--build-deps',
                        action='store_true', dest='build_deps',
                        help='check and install build dependencies')
    parser.add_argument('-y', '--yes',
                        action='store_true', dest='assumeyes',
                        help='always assume yes to any prompted question')
    parser.add_argument('path_or_url', nargs='?',
                        help='path or url to project providing mmpack package')
    args = parser.parse_args(argv)

    if not args.path_or_url:
        args.path_or_url = find_project_root_folder()
        if not args.path_or_url:
            raise ValueError('did not find project to package')

    # set workspace prefix
    if not args.prefix:
        try:
            args.prefix = os.environ['MMPACK_PREFIX']
        except KeyError:
            pass
    if args.prefix:
        Workspace().prefix = os.path.abspath(args.prefix)

    return args


def _is_git_dir(path: str) -> bool:
    if os.path.isdir(path + '/.git'):
        return True

    retcode = call(['git', '--git-dir='+path, 'rev-parse', '--git-dir'],
                   stdin=DEVNULL, stdout=DEVNULL, stderr=DEVNULL)
    return retcode == 0


def _is_tarball_file(path: str) -> bool:
    try:
        with tarfile.open(path, 'r') as tar:
            return 'mmpack/specs' in tar.getnames()
    # pylint: disable=broad-except
    except Exception:
        return False


def _guess_method(path_or_url: str) -> str:
    if _is_git_dir(path_or_url):
        return 'git'
    elif os.path.isdir(path_or_url):
        return 'path'
    elif _is_tarball_file(path_or_url):
        if path_or_url.endswith('_src.tar.xz'):
            return 'srcpkg'
        else:
            return 'tar'

    # If nothing has worked before, lets assume this is a git remote url
    return 'git'


def main(argv):
    """
    entry point to create a mmpack package
    """
    args = parse_options(argv[1:])
    if not args.method:
        args.method = _guess_method(args.path_or_url)

    srctarball = SourceTarball(args.method, args.path_or_url, args.tag)
    srctarball.prepare_binpkg_build()

    specfile = os.path.join(srctarball.detach_srcdir(), 'mmpack/specs')
    package = SrcPackage(specfile, srctarball.tag, srctarball.srctar)

    if args.build_deps:
        package.install_builddeps(prefix=args.prefix, assumeyes=args.assumeyes)

    set_log_file(package.pkgbuild_path() + '/mmpack.log')

    package.local_install(args.skip_tests)
    package.ventilate()
    package.generate_binary_packages()
