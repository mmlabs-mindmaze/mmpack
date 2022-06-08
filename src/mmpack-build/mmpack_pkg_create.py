# @mindmaze_header@
"""
Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.
"""

from argparse import Action, ArgumentParser, Namespace

from .common import wprint
from .mmpack_mksource import (add_parser_args as add_mksource_parser_argument,
                              call_foreach_srcpkg)
from .prefix import new_mmpack_prefix_context
from .src_package import SrcPackage


class DeprecatedStoreAction(Action):
    def __call__(self, parser, namespace, values, option_string=None):
        wprint(f'Option {option_string} of {parser.prog} is deprecated')
        setattr(namespace, self.dest, values)


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with pkg-create related arguments"""
    add_mksource_parser_argument(parser)
    parser.add_argument('-p', '--prefix',
                        action=DeprecatedStoreAction, dest='prefix', type=str,
                        help='prefix within which to work (DEPRECATED)')
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    parser.add_argument('--build-deps',
                        action='store_true', dest='build_deps',
                        help='check and install build dependencies')


def _pkg_create_build(package: SrcPackage, args: Namespace):
    with new_mmpack_prefix_context(package.pkgbuild_path() + '/deps_prefix'):
        if args.build_deps or args.repo_url:
            package.install_builddeps()

        package.build_binpkgs(args.skip_tests)


def main(args):
    """
    entry point to create a mmpack package
    """
    return call_foreach_srcpkg(_pkg_create_build, args)
