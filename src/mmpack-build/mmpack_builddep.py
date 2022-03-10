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

from . common import get_host_dist, run_cmd, specs_load
from . errors import MMPackBuildError
from . workspace import find_project_root_folder, Workspace


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
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')


def general_specs_builddeps(general):
    """
    extract the system and mmpack build dependencies from the specs

    Args:
        general: content of general section of the specs only

    Returns:
        tuple of list of system build depeds and list of mmpack build
        depends
    """
    build_sysdeps_key = 'build-depends-' + get_host_dist()
    mmpack_builddeps = []
    system_builddeps = []

    if 'build-depends' in general:
        mmpack_builddeps += general['build-depends']

    if build_sysdeps_key in general:
        sysbuilddeps = general[build_sysdeps_key]
        if 'mmpack' in sysbuilddeps:
            mmpack_builddeps.append(sysbuilddeps['mmpack'])
        if 'system' in sysbuilddeps:
            system_builddeps = sysbuilddeps['system']

    return system_builddeps, mmpack_builddeps


def process_dependencies(system_builddeps, mmpack_builddeps,
                         prefix: str):
    """
    process given dependencies

    1/ check sysdeps for presence
    2/ install mmpack deps if missing

    Raises:
        ShellException: the called program returned failure code
    """
    mmpack_bin = Workspace().mmpack_bin()

    # check sysdeps first
    if system_builddeps:
        run_cmd([mmpack_bin, 'check-sysdep'] + system_builddeps)

    if not mmpack_builddeps:
        return

    # install missing mmpack packages
    # forward options to mmpack install
    cmd = [mmpack_bin]
    if prefix:
        cmd.append('--prefix=' + prefix)
    cmd.append('install')
    cmd.append('-y')

    cmd += mmpack_builddeps
    run_cmd(cmd)


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

    system_builddeps, mmpack_builddeps = general_specs_builddeps(specs)

    try:
        process_dependencies(system_builddeps, mmpack_builddeps,
                             options.prefix)
    except MMPackBuildError as err:
        print(str(err), file=sys.stderr)
        return 1

    return 0
