# @mindmaze_header@
'''
install source package dependencies.

TODOC
'''

import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from subprocess import run

from common import get_host_dist, yaml_load, dprint
from settings import LIBEXECDIR
from workspace import find_project_root_folder


def parse_option(argv):
    'parse options'
    parser = ArgumentParser(description=__doc__,
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


def process_dependencies(system_builddeps, mmpack_builddeps,
                         prefix: str, assumeyes: bool):
    ''' process given dependencies

    1/ check sysdeps for presence
    2/ install mmpack deps if missing
    '''
    # check sysdeps first
    if system_builddeps:
        sysdep_cmd = [LIBEXECDIR + '/mmpack/mmpack-check-sysdep']
        sysdep_cmd += system_builddeps
        ret = run(sysdep_cmd)
        if ret.returncode:
            return ret.returncode

    if not mmpack_builddeps:
        return 0

    # install missing mmpack packages
    # forward options to mmpack install
    cmd = 'mmpack'
    if prefix:
        cmd += ' --prefix=' + prefix
    cmd += ' install '
    if assumeyes:
        cmd += '-y '

    cmd += ' '.join(mmpack_builddeps)
    dprint('[shell] {0}'.format(cmd))
    ret = run(cmd, shell=True)

    return ret.returncode


def main():
    'main function'
    options = parse_option(sys.argv[1:])
    specs = yaml_load(options.specfile)['general']

    mmpack_builddeps = []
    system_builddeps = []

    if 'build-depends' in specs:
        mmpack_builddeps += specs['build-depends']

    build_sysdeps_key = 'build-depends-' + get_host_dist()

    # append platform-specific mmpack packages
    # eg. one required package is not available on this platform
    if build_sysdeps_key in specs:
        sysbuilddeps = specs[build_sysdeps_key]
        if 'mmpack' in sysbuilddeps:
            mmpack_builddeps.append(sysbuilddeps['mmpack'])
        if 'system' in sysbuilddeps:
            system_builddeps = sysbuilddeps['system']

    return process_dependencies(system_builddeps, mmpack_builddeps,
                                options.prefix, options.assumeyes)


if __name__ == '__main__':
    main()
