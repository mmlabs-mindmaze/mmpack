# @mindmaze_header@
"""
mmpack-build is a stub for all the tools required to create mmpack packages.

For a list of all mmpack-build commands
>>> mmpack-build --list-commands
"""

import sys

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


def _help():
    print('''
          This is a stub for all mmpack-build commands

          Usage:
          $ mmpack-build <command> [command-options]

          List all mmpack commands:
          $ mmpack-build --list-commands

          More infos about a specific command:
          $ mmpack-build <command> --help

          List of available commands:
          ''')
    for subcmd in ALL_CMDS:
        print('\t' + subcmd.CMD)


def main():
    """
    main entry point for all mmpack-build commands, and only a stub
    redirecting to the various mmpack-bulid commands
    """
    # make pkg-create the default target
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        args = [cmd] + sys.argv[2:]
    else:
        cmd = mmpack_pkg_create.CMD
        args = [cmd]

    # execute requested mmpack-build subcommand
    for subcmd in ALL_CMDS:
        if cmd == subcmd.CMD:
            exit(subcmd.main(args))

    # unrecognized mmpack-build subcommand
    _help()
    exit(-1)


if __name__ == "__main__":
    main()
