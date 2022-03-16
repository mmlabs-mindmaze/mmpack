# @mindmaze_header@
"""
Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.
"""

import os
import sys
from argparse import ArgumentParser

from . errors import MMPackBuildError
from . mmpack_mksource import add_parser_args as add_mksource_parser_argument
from . src_package import SrcPackage
from . workspace import Workspace
from . source_tarball import SourceTarball
from . xdg import XDG_DATA_HOME


def _get_prefix_abspath(prefix) -> str:
    if not ((os.sep in prefix)
            or (os.altsep is not None and os.altsep in prefix)):
        return XDG_DATA_HOME + '/mmpack/prefix/' + prefix

    return os.path.abspath(prefix)


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with pkg-create related arguments"""
    add_mksource_parser_argument(parser)
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


def check_options(args):
    """
    check options
    """
    # set workspace prefix
    if not args.prefix:
        try:
            args.prefix = os.environ['MMPACK_PREFIX']
        except KeyError:
            pass
    if args.prefix:
        Workspace().prefix = _get_prefix_abspath(args.prefix)

    return args


def _build_mmpack_packages(srctar: str, tag: str, srcdir: str, args):
    package = SrcPackage(srctar, tag, srcdir)

    if args.build_deps:
        package.install_builddeps()

    package.build_binpkgs(args.skip_tests)


def main(args):
    """
    entry point to create a mmpack package
    """
    retcode = 0
    args = check_options(args)
    srctarball = SourceTarball(args.method, args.path_or_url, args.tag,
                               build_only_modified=args.only_modified,
                               version_from_vcs=args.update_version_from_vcs)
    for prj_src in srctarball.iter_mmpack_srcs():
        try:
            _build_mmpack_packages(prj_src.tarball, srctarball.tag,
                                   prj_src.srcdir, args)
        except MMPackBuildError as err:
            print(f'Build of {prj_src.name} failed: {err}', file=sys.stderr)
            retcode = 1

    return retcode
