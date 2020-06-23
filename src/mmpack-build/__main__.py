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
from . import mmpack_pkg_create
from . import common
from . settings import PACKAGE_VERSION


# all subcommand MUST expose:
#  - CMD: the subcommand command name (string)
#  - main: the subcommand main entry point (function)
ALL_CMDS = {
    mmpack_builddep,
    mmpack_clean,
    mmpack_guess,
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
    args = [command] + args
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
                        ret = 0 if sysexit.code is None else sysexit.code
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
    cmd_choices = ['list-commands'] + [subcmd.CMD for subcmd in ALL_CMDS]
    parser = ArgumentParser(prog='mmpack-build', add_help=False,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('command', nargs='?', choices=cmd_choices,
                        help='execute sub-command')
    parser.add_argument("-h", "--help", help="show this help message and exit",
                        action="store_true", default=False)
    parser.add_argument("-v", "--version", help="show version and exit",
                        action="store_true", default=False)
    parser.add_argument("-q", "--quiet", help="silence output",
                        action="store_true", default=False)
    parser.add_argument("-d", "--debug", help="toggle debug mode",
                        action="store_true", default=False)

    args, subargs = parser.parse_known_args()
    common.CONFIG['verbose'] = not args.quiet
    common.CONFIG['debug'] = args.debug

    if args.help or args.version:  # handle flags
        if args.command:
            # sub-command flags in common with this script must be re-added
            ret = launch_subcommand(args.command, ['--help'] + subargs)
        elif args.help:
            parser.print_help()
        else:  # args.version
            print('mmpack-build', PACKAGE_VERSION)
    else:  # launch sub-command
        if not args.command:
            args.command = 'pkg-create'  # default sub-command
        ret = launch_subcommand(args.command, subargs)

    return ret


if __name__ == "__main__":
    sys.exit(main())
