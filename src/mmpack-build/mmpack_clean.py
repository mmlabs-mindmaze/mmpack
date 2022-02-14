# @mindmaze_header@
"""
Small helper used to ensure a clean state of mmpack-build work folders
"""

from argparse import ArgumentParser, RawDescriptionHelpFormatter

from . workspace import Workspace


CMD = 'clean'


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with clean related arguments"""
    parser.add_argument('--wipe',
                        action='store_true',
                        help='wipe all files including generated packages')


def main(argv):
    """
    helper to clean the mmpack generated files
    """
    # pylint: disable=invalid-name
    parser = ArgumentParser(description=__doc__,
                            prog='mmpack-build ' + CMD,
                            formatter_class=RawDescriptionHelpFormatter)
    add_parser_args(parser)
    options = parser.parse_args(argv[1:])

    if options.wipe:
        Workspace().wipe()
    else:
        Workspace().clean()
