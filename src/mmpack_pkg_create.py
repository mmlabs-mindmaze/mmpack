# @mindmaze_header@
'''
Create a mmpack package

Usage:
mmpack pkg-create [--git-url <path or url of a git repo> | --src <tarball>]
                  [--tag <tag>] [--prefix <prefix>] [--skip-build-tests]

If neither git url or source tarball was given, look through the tree for a
mmpack folder, and use the containing folder as root directory.

Otherwise, if git repository url is provided, clone the project from given
git url. If a tag was specified, use it (checkout on the given tag).

If source tarball is specified, build packages from the unpacked archive.
If tag is specified, it will indicated the name of sub folder in the cache
to use to build the packages.

If a prefix is given, work within it instead.

Examples:
# From any subfolder of the project
$ mmpack pkg-create

# From anywhere
$ mmpack pkg-create --url ssh://git@intranet.mindmaze.ch:7999/~user/XXX.git \
                    --tag v1.0.0-custom-tag-target
'''

import os
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from glob import glob

from common import pushdir, popdir, yaml_load
from src_package import create_source_from_git, load_source_from_tar
from workspace import Workspace, find_project_root_folder


def read_from_mmpack_files(root_dir: str, key: str) -> str:
    ''' Look for given key's value in the mmpack specfile of the project

        Scan given project's mmpack folder. Try to read all the yaml files
        as the project specfile (we might be guessing the project's name).

        Expects the key to be a root key of the yaml file
    '''
    try:
        pushdir(root_dir + '/mmpack')
    except FileNotFoundError:
        return None

    for specfile in glob('*.yaml'):
        specs = yaml_load(specfile)
        if key in specs:
            return specs[key]

    popdir()  # root_dir
    return None


def parse_options(argv):
    'parse and check options'
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--git-url',
                       action='store', dest='url', type=str,
                       help='project git url/path')
    group.add_argument('--src',
                       action='store', dest='srctar', type=str,
                       help='source package tarball')
    parser.add_argument('-t', '--tag',
                        action='store', dest='tag', type=str,
                        help='project tag')
    parser.add_argument('-p', '--prefix',
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    parser.add_argument('--build-deps',
                        action='store_true', dest='build_deps',
                        help='check and install build dependencies')
    parser.add_argument('-y', '--yes',
                        action='store_true', dest='assumeyes',
                        help='always assume yes to any prompted question')
    args = parser.parse_args(argv)

    if not args.url and not args.srctar:
        args.url = find_project_root_folder()
        if not args.url:
            raise ValueError('did not find project to package')

    # set workspace prefix
    if not args.prefix:
        try:
            args.prefix = os.environ['MMPACK_PREFIX']
        except KeyError:
            pass
    if args.prefix:
        Workspace().prefix = args.prefix

    return args


def main():
    'entry point to create a mmpack package'
    args = parse_options(sys.argv[1:])

    if args.url:
        package = create_source_from_git(url=args.url, tag=args.tag)
    else:
        package = load_source_from_tar(tarpath=args.srctar, tag=args.tag)

    if args.build_deps:
        package.install_builddeps(prefix=args.prefix, assumeyes=args.assumeyes)

    package.local_install(args.skip_tests)
    package.ventilate()
    package.generate_binary_packages()


if __name__ == '__main__':
    main()
