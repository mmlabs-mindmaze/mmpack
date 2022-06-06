# @mindmaze_header@
"""
Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.
"""

from argparse import ArgumentParser, Namespace

from .mmpack_mksource import (add_parser_args as add_mksource_parser_argument,
                              call_foreach_srcpkg)
from .prefix import new_prefix, prefix_create
from .src_package import SrcPackage


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with pkg-create related arguments"""
    add_mksource_parser_argument(parser)
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    parser.add_argument('-r', '--repo-url', dest='repo_url', action='append',
                        default=[],
                        help='URL of the repository to fetch dependencies '
                             'from. Can be supplied multiple times to use '
                             'several repositories.')


def _pkg_build(package: SrcPackage, args: Namespace):
    # Set mmpack prefix location for the rest of package build
    with new_prefix(package.pkgbuild_path() + '/deps_prefix'):

        # Create mmpack prefix
        prefix_create(args.repo_url)

        # Install build dependencies in the newly created prefix
        package.install_builddeps()

        # Build packages
        package.build_binpkgs(args.skip_tests)


def main(args):
    """
    entry point to create a mmpack package with temporary prefix
    """
    return call_foreach_srcpkg(_pkg_build, args)
