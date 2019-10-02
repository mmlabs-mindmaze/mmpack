# @mindmaze_header@
"""
Small helper used to ensure a clean state of mmpack-build work folders
"""

from argparse import ArgumentParser, RawDescriptionHelpFormatter

from . workspace import Workspace


CMD = 'clean'


def main(argv):
    """
    helper to clean the mmpack generated files
    """
    # pylint: disable=invalid-name
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--wipe',
                        action='store_true',
                        help='wipe all files including generated packages')

    options = parser.parse_args(argv[1:])

    if options.wipe:
        Workspace().wipe()
    else:
        Workspace().clean()
