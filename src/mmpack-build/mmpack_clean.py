# @mindmaze_header@
"""
Small helper used to ensure a clean state of mmpack-build work folders
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter

from workspace import Workspace


if __name__ == '__main__':
    # pylint: disable=invalid-name
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--wipe',
                        action='store_true',
                        help='wipe all files including generated packages')

    options = parser.parse_args(sys.argv[1:])

    if options.wipe:
        Workspace().wipe()
    else:
        Workspace().clean()
