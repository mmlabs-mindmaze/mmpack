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
    mmpack_builddep,
    mmpack_clean,
    mmpack_guess,
    mmpack_mksource,
    mmpack_pkg_create,
}


def _list_commands():
    for subcmd in ALL_CMDS:
        print('\t' + subcmd.CMD)
    print('\tlist-commands')


def launch_subcommand(command, args):
    """
    wrapper for calling the sub-commands which handles the special case of
    'list-commands' subcommand.
    The wrapper also masks Exceptions, hiding the backtrace when debug mode is
    disabled.
    """
    # pylint: disable=broad-except
    ret = 127  # command not found error
    if command == 'list-commands':
        _list_commands()
    else:
        for subcmd in ALL_CMDS:
            if command == subcmd.CMD:
                if common.CONFIG['debug']:
                    ret = subcmd.main(args)
                else:
                    try:
                        ret = subcmd.main(args)
                    except KeyboardInterrupt:
                        ret = 130
                    except SystemExit as sysexit:
                        # pylint: disable=using-constant-test
                        ret = sysexit.code if sysexit.code else 0
                    except Exception as inst:
                        print('Exception: ', inst)
                        ret = 1
    return ret


def main():
    """
    main entry point for all mmpack-build commands, and only a stub
    redirecting to the various mmpack-bulid commands
    """
    ret = 0
    parser = ArgumentParser(prog='mmpack-build',
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument("-v", "--version", help="show version and exit",
                        action="store_true", default=False)
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
    subparsers = parser.add_subparsers(dest='command')
    for mod in ALL_CMDS:
        subparser = subparsers.add_parser(mod.CMD)
        mod.add_parser_args(subparser)

    args = parser.parse_args()
    common.CONFIG['verbose'] = not args.quiet
    common.CONFIG['debug'] = args.debug

    wrk = Workspace()
    if args.outdir:
        wrk.set_outdir(args.outdir)
    if args.builddir:
        wrk.set_builddir(args.builddir)
    if args.cachedir:
        wrk.set_cachedir(args.cachedir)

    if args.version:
        print('mmpack-build', PACKAGE_VERSION)
        return 0

    return launch_subcommand(args.command, args)


if __name__ == "__main__":
    sys.exit(main())
