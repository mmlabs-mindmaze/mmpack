# @mindmaze_header@
"""
Create mmpack package specified in argument. If no argument is provided, look
through the tree for a mmpack folder, and use the containing folder as root
directory.
"""

import os
from argparse import ArgumentParser, Namespace

from . mmpack_mksource import (add_parser_args as add_mksource_parser_argument,
                               call_foreach_srcpkg)
from . src_package import SrcPackage
from . workspace import Workspace
from . xdg import XDG_DATA_HOME


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with pkg-create related arguments"""
    add_mksource_parser_argument(parser)
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')


def _pkg_build(package: SrcPackage, args: Namespace):
    #Create mmpack prefix here
    package.install_builddeps()

    package.build_binpkgs(args.skip_tests)


def main(args):
    """
    entry point to create a mmpack package with temporary prefix
    """
    return call_foreach_srcpkg(_pkg_build, args)
