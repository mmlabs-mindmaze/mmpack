# @mindmaze_header@
"""
Install given source package build dependencies.
Uses current repository to find mmpack specfile if none given.

System packages will be checked for only; command will fail if any
system dependency is unmet. All the available mmpack packages found
missing will be proposed for install within the current prefix.
"""

from argparse import ArgumentParser, RawDescriptionHelpFormatter
from subprocess import run

from . common import get_host_dist, yaml_load, dprint, shell
from . workspace import find_project_root_folder, Workspace


CMD = 'builddep'


def parse_option(argv):
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            prog='mmpack-build ' + CMD,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('specfile', type=str, nargs='?',
                        default=find_project_root_folder() + '/mmpack/specs',
                        help='path to the specfile')

    # XXX: assume-yes is True even if not set if there is only one package
    # staged for install
    parser.add_argument('-y', '--yes',
                        action='store_true', dest='assumeyes',
                        help='indicate that build tests must not be run')
    parser.add_argument('-p', '--prefix',
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')
    return parser.parse_args(argv)


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
                         prefix: str, assumeyes: bool):
    """
    process given dependencies

    1/ check sysdeps for presence
    2/ install mmpack deps if missing

    raises:
        ShellException: one of the command failed
    """
    mmpack_bin = Workspace().mmpack_bin()

    # check sysdeps first
    if system_builddeps:
        shell([mmpack_bin, 'check-sysdep'] + system_builddeps)

    if not mmpack_builddeps:
        return 0

    # install missing mmpack packages
    # forward options to mmpack install
    cmd = [mmpack_bin]
    if prefix:
        cmd.append('--prefix=' + prefix)
    cmd.append('install')
    if assumeyes:
        cmd.append('-y')

    cmd += mmpack_builddeps
    shell(cmd)


def main(argv):
    """
    main function
    """
    options = parse_option(argv[1:])
    specs = yaml_load(options.specfile)['general']

    system_builddeps, mmpack_builddeps = general_specs_builddeps(specs)

    try:
        process_dependencies(system_builddeps, mmpack_builddeps,
                             options.prefix, options.assumeyes)
    except ShellException:
        return 1
