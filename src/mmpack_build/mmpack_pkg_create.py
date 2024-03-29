# @mindmaze_header@
"""
Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.
"""

from argparse import ArgumentParser, Namespace, SUPPRESS

from .common import DeprecatedStoreAction
from .mmpack_mksource import (add_parser_args as add_mksource_parser_argument,
                              call_foreach_srcpkg)
from .prefix import new_mmpack_prefix_context
from .src_package import SrcPackage


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with pkg-create related arguments"""
    add_mksource_parser_argument(parser)
    parser.add_argument('-p', '--prefix',
                        action=DeprecatedStoreAction, dest='prefix', type=str,
                        default=SUPPRESS,
                        help='prefix within which to work (DEPRECATED)')
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    parser.add_argument('--build-deps',
                        action='store_const', dest='build_deps', const=True,
                        help='install build dependencies (DEPRECATED)')
    parser.add_argument('-y', '--yes',
                        action='store_true', dest='assumeyes',
                        help='always assume yes to any prompted question')


def _pkg_create_build(package: SrcPackage, args: Namespace):
    with new_mmpack_prefix_context(package.pkgbuild_path() + '/deps_prefix'):
        package.install_builddeps()
        package.build_binpkgs(args.skip_tests)


def main(args):
    """
    entry point to create a mmpack package
    """
    return call_foreach_srcpkg(_pkg_create_build, args)
