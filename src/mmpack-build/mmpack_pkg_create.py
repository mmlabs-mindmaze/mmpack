# @mindmaze_header@
"""
Create a mmpack package

Usage:

mmpack pkg-create [--git-url <path or url of a git repo> | --src <tarball> |
                   --mmpack-src <mmpack_source_tarball>]
                  [--tag <tag>] [--prefix <prefix>] [--skip-build-tests]

If neither git url or source tarball was given, look through the tree for a
mmpack folder, and use the containing folder as root directory.

Otherwise, if git repository url is provided, clone the project from given
git url. If a tag was specified, use it (checkout on the given tag).

If source tarball is specified, build packages from the unpacked archive.
If tag is specified, it will indicated the name of sub folder in the cache
to use to build the packages.

If a prefix is given, work within it instead.

Examples:
# From any subfolder of the project
$ mmpack pkg-create

# From anywhere
$ mmpack pkg-create --url ssh://git@intranet.mindmaze.ch:7999/~user/XXX.git \
                    --tag v1.0.0-custom-tag-target
"""

import os
from argparse import ArgumentParser, RawDescriptionHelpFormatter

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
    group.add_argument('--git-url',
                       action='store', dest='url', type=str,
                       help='project git url/path')
    group.add_argument('--path',
                       action='store', dest='path', type=str,
                       help='path to folder having containing a '
                            'mmpack dir containing specs')
    group.add_argument('--src',
                       action='store', dest='srctar', type=str,
                       help='source package tarball')
    group.add_argument('--mmpack-src',
                       action='store', dest='mmpack_srctar', type=str,
                       help='mmpack source package tarball')
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
    args = parser.parse_args(argv)

    if not args.url and not args.srctar and not args.mmpack_srctar \
       and not args.path:
        args.url = find_project_root_folder()
        if not args.url:
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


def main(argv):
    """
    entry point to create a mmpack package
    """
    args = parse_options(argv[1:])

    if args.url:
        method = 'git'
        path_url = args.url
    elif args.srctar:
        method = 'tar'
        path_url = args.srctar
    elif args.mmpack_srctar:
        method = 'srcpkg'
        path_url = args.mmpack_srctar
    elif args.path:
        method = 'path'
        path_url = args.path

    srctarball = SourceTarball(method, path_url, args.tag)
    srctarball.prepare_binpkg_build()

    specfile = os.path.join(srctarball.detach_srcdir(), 'mmpack/specs')
    package = SrcPackage(specfile, srctarball.tag, srctarball.srctar)

    if args.build_deps:
        package.install_builddeps(prefix=args.prefix, assumeyes=args.assumeyes)

    set_log_file(package.pkgbuild_path() + '/mmpack.log')

    package.local_install(args.skip_tests)
    package.ventilate()
    package.generate_binary_packages()
