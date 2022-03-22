# @mindmaze_header@
"""
mmpack-build is a stub for all the tools required to create mmpack packages.

For a list of all mmpack-build commands
>>> mmpack-build list-commands
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter

from . import mmpack_builddep
from . import mmpack_clean
from . import mmpack_create_with_repos
from . import mmpack_guess
from . import mmpack_mksource
from . import mmpack_pkg_create
from . import common
from . settings import PACKAGE_VERSION
from . workspace import Workspace


# all subcommand MUST expose:
#  - CMD: the subcommand command name (string)
#  - main: the subcommand main entry point (function)
ALL_CMDS = {
    'builddep': mmpack_builddep,
    'create-with-repos': mmpack_create_with_repos,
    'clean': mmpack_clean,
    'guess': mmpack_guess,
    'mksource': mmpack_mksource,
    'pkg-create': mmpack_pkg_create,
}


def cmdline_parser() -> ArgumentParser:
    """Create cmdline parser of mmpack-build and subcmds"""
    parser = ArgumentParser(prog='mmpack-build',
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument("-v", "--version", help="show version and exit",
                        action="version", version=PACKAGE_VERSION)
    parser.add_argument("-q", "--quiet", help="silence output",
                        action="store_true", default=False)
    parser.add_argument("-d", "--debug", help="toggle debug mode",
                        action="store_true", default=False)
    parser.add_argument("-o", "--outdir", help="output folder",
                        action="store", dest='outdir', nargs='?')
    parser.add_argument("--builddir", help="build folder",
                        action="store", dest='builddir', nargs='?')
    parser.add_argument("--cachedir", help="cache folder",
                        action="store", dest='cachedir', nargs='?')

    subparsers = parser.add_subparsers(dest='command', required=True)
    subparsers.add_parser('list-commands')
    for subcmd, mod in ALL_CMDS.items():
        mod.add_parser_args(subparsers.add_parser(subcmd, help=mod.__doc__))

    return parser


def _list_commands():
    for subcmd in ALL_CMDS:
        print('\t' + subcmd)
    print('\tlist-commands')
    return 0


def launch_subcommand(args):
    """
    wrapper for calling the sub-commands which handles the special case of
    'list-commands' subcommand.
    The wrapper also masks Exceptions, hiding the backtrace when debug mode is
    disabled.
    """
    if args.command == 'list-commands':
        return _list_commands()

    mod = ALL_CMDS[args.command]
    if common.CONFIG['debug']:
        return mod.main(args)
    else:
        try:
            ret = mod.main(args)
        except KeyboardInterrupt:
            ret = 130
        except SystemExit as sysexit:
            # pylint: disable=using-constant-test
            ret = sysexit.code if sysexit.code else 0
        except Exception as inst:  # pylint: disable=broad-except
            print('Exception: ', inst)
            ret = 1

    return ret


def main():
    """
    main entry point for all mmpack-build commands, and only a stub
    redirecting to the various mmpack-bulid commands
    """
    args = cmdline_parser().parse_args()
    common.CONFIG['verbose'] = not args.quiet
    common.CONFIG['debug'] = args.debug

    wrk = Workspace()
    if args.outdir:
        wrk.set_outdir(args.outdir)
    if args.builddir:
        wrk.set_builddir(args.builddir)
    if args.cachedir:
        wrk.set_cachedir(args.cachedir)

    return launch_subcommand(args)


if __name__ == "__main__":
    sys.exit(main())
