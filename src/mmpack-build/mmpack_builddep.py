# @mindmaze_header@
"""
Install given source package build dependencies.
Uses current repository to find mmpack specfile if none given.

System packages will be checked for only; command will fail if any
system dependency is unmet. All the available mmpack packages found
missing will be proposed for install within the current prefix.
"""

import sys
from argparse import ArgumentParser

from .common import DeprecatedStoreAction, specs_load
from .errors import MMPackBuildError
from .prefix import prefix_install
from .workspace import find_project_root_folder


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with builddeps related arguments"""
    parser.add_argument('specfile', type=str, nargs='?',
                        help='path to the specfile')

    # XXX: assume-yes is True even if not set if there is only one package
    # staged for install
    parser.add_argument('-y', '--assume-yes',
                        action='store_true', dest='assumeyes',
                        help='Assume yes as answer to all prompts and run'
                             ' non-interactively.')
    parser.add_argument('-p', '--prefix',
                        action=DeprecatedStoreAction, dest='prefix', type=str,
                        help='prefix within which to work (DEPRECATED)')


def main(options):
    """
    main function
    """
    if options.specfile is not None:
        specfile = options.specfile
    else:
        prj_root = find_project_root_folder()
        if not prj_root:
            raise MMPackBuildError('did not find project to package')
        specfile += '/mmpack/specs'
    specs = specs_load(specfile)

    mmpack_builddeps = specs.get('build-depends', [])

    try:
        # install missing mmpack packages
        prefix_install(mmpack_builddeps)
    except MMPackBuildError as err:
        print(str(err), file=sys.stderr)
        return 1

    return 0
