# @mindmaze_header@
"""
mmpack-build is a stub for all the tools required to create mmpack packages.
"""

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from io import TextIOBase
from pathlib import Path
from typing import Union

try:
    from argcomplete import autocomplete
except ImportError:
    # pylint: disable=missing-function-docstring,unused-argument
    def autocomplete(parser: ArgumentParser, **kwargs):
        pass

from . import mmpack_builddep
from . import mmpack_clean
from . import mmpack_guess
from . import mmpack_mksource
from . import mmpack_pkg_create
from . import common
from .prefix import BuildPrefix, configure_prefix_handling
from .workspace import Workspace


try:
    with open(Path(__file__).parent / 'VERSION', encoding='utf-8') as fp:
        _PACKAGE_VERSION = fp.read().strip()
except FileNotFoundError:
    _PACKAGE_VERSION = 'unkown'


# all subcommand MUST expose a main function, the subcommand main entry point
ALL_CMDS = {
    'builddep': mmpack_builddep,
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
                        action="version", version=_PACKAGE_VERSION)
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
    parser.add_argument('-p', '--prefix',
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')
    parser.add_argument('-r', '--repo-url', dest='repo_url',
                        action='append', default=[],
                        help='URL of the repository to fetch dependencies '
                             'from. Can be supplied multiple times to use '
                             'several repositories.')
    parser.add_argument('--install-deps',
                        action='store_const', dest='install_deps', const=True,
                        help='install dependencies necessary for the build')
    parser.add_argument('--no-install-deps',
                        action='store_const', dest='install_deps', const=False,
                        help='do not install dependencies')
    parser.add_argument('--use-build-prefix', dest='build_prefix',
                        choices=[e.value for e in BuildPrefix],
                        help='prefix to use during build')

    subparsers = parser.add_subparsers(dest='command', required=False)
    for subcmd, mod in ALL_CMDS.items():
        mod.add_parser_args(subparsers.add_parser(subcmd, help=mod.__doc__))

    return parser


def launch_subcommand(args):
    """
    wrapper for calling the sub-commands.
    The wrapper also masks Exceptions, hiding the backtrace when debug mode is
    disabled.
    """
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


class MixedIO:
    """File-like accepting both bytes and str"""

    def __init__(self, stream: TextIOBase):
        self._file = stream

    def write(self, data: Union[bytes, str]) -> int:
        """Same as IOBase.write"""
        if isinstance(data, bytes):
            data = data.decode()
        return self._file.write(data)

    def flush(self):
        """Same as IOBase.flush"""
        self._file.flush()


def main():
    """
    main entry point for all mmpack-build commands, and only a stub
    redirecting to the various mmpack-bulid commands
    """
    parser = cmdline_parser()
    autocomplete(parser, output_stream=MixedIO(sys.stdout))
    args = parser.parse_args()

    # fake default command set to pkg-create. For some obscure reason,
    # parser.set_defaults() does not work for subparsers.
    if args.command is None:
        args = parser.parse_args(['pkg-create'], args)

    common.CONFIG['verbose'] = not args.quiet
    common.CONFIG['debug'] = args.debug

    wrk = Workspace()
    if args.outdir:
        wrk.set_outdir(args.outdir)
    if args.builddir:
        wrk.set_builddir(args.builddir)
    if args.cachedir:
        wrk.set_cachedir(args.cachedir)
    if args.prefix:
        wrk.set_prefix(args.prefix)
    configure_prefix_handling(args)

    retval = launch_subcommand(args)

    wrk.cleanup_cache()
    return retval


if __name__ == "__main__":
    sys.exit(main())
