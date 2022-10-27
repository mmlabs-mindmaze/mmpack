# @mindmaze_header@
"""
Small helper used to ensure a clean state of mmpack-build work folders
"""

from argparse import ArgumentParser

from .workspace import Workspace


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with clean related arguments"""
    parser.add_argument('--wipe',
                        action='store_true',
                        help='wipe all files including generated packages')


def main(options):
    """
    helper to clean the mmpack generated files
    """
    if options.wipe:
        Workspace().wipe()
    else:
        Workspace().clean()
