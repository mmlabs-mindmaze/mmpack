# @mindmaze_header@
"""
mmpack-build is a stub for all the tools required to create mmpack packages.

For a list of all mmpack-build commands
>>> mmpack-build list-commands
"""

from argparse import ArgumentParser, RawDescriptionHelpFormatter

from . import mmpack_builddep
from . import mmpack_clean
from . import mmpack_pkg_create


# all subcommand MUST expose:
#  - CMD: the subcommand command name (string)
#  - main: the subcommand main entry point (function)
ALL_CMDS = {
    mmpack_builddep,
    mmpack_clean,
    mmpack_pkg_create,
}


def _list_commands():
    for subcmd in ALL_CMDS:
        print('\t' + subcmd.CMD)
    print('\tlist-commands')


def launch_subcommand(command, args):
    """
    wrapper for calling the sub-commands which handles the special case of
    'list-commands' subcommands.
    """
    ret = 1
    args = [command] + args
    if command == 'list-commands':
        _list_commands()
    else:
        for subcmd in ALL_CMDS:
            if command == subcmd.CMD:
                ret = subcmd.main(args)
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

    args, subargs = parser.parse_known_args()

    if args.help:  # show help
        if args.command:
            # sub-command flags in common with this script must be re-added
            ret = launch_subcommand(args.command, ['--help'] + subargs)
        else:
            parser.print_help()
    else:  # launch sub-command
        if not args.command:
            args.command = 'pkg-create'  # default sub-command
        ret = launch_subcommand(args.command, subargs)

    return ret


if __name__ == "__main__":
    exit(main())
