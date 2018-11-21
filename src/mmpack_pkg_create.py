# @mindmaze_header@
'''
Create a mmpack package

Usage:
mmpack pkg-create [--url <path or git url>] [--tag <tag>]
                  [--prefix <prefix>] [--skip-build-tests]

If no url was given, look through the tree for a mmpack folder, and use the
containing folder as root directory.

Otherwise, clone the project from given git url.
If a tag was specified, use it (checkout on the given tag).

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

from common import shell, pushdir, popdir, yaml_load
from src_package import SrcPackage
from workspace import Workspace


def find_project_root_folder() -> str:
    ''' Look for folder named 'mmpack' in the current directory
        or any parent folder.
    '''
    pwd = os.getcwd()
    if os.path.exists('mmpack'):
        return pwd

    parent, current = os.path.split(pwd)
    if not current:
        return None
    pushdir(parent)
    root_folder = find_project_root_folder()
    popdir()
    return root_folder


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
    parser.add_argument('-s', '--url',
                        action='store', dest='url', type=str,
                        help='project git url')
    parser.add_argument('-t', '--tag',
                        action='store', dest='tag', type=str,
                        help='project tag')
    parser.add_argument('-p', '--prefix',
                        action='store', dest='prefix', type=str,
                        help='prefix within which to work')
    parser.add_argument('--skip-build-tests',
                        action='store_true', dest='skip_tests',
                        help='indicate that build tests must not be run')
    args = parser.parse_args(argv)

    ctx = {'url': args.url,
           'tag': args.tag,
           'prefix': args.prefix,
           'skip_tests': args.skip_tests}

    if not ctx['url']:
        ctx['url'] = find_project_root_folder()
        if not ctx['url']:
            raise ValueError('did not find project to package')
    if not ctx['tag']:
        # use current branch name
        ctx['tag'] = shell('git rev-parse --abbrev-ref HEAD').strip()

    # create a srcname from the arguments
    # remove trailing path separators
    if ctx['url'].endswith('.git'):
        ctx['srcname'] = os.path.basename(ctx['url'])
        ctx['srcname'] = ctx['srcname'][:-4]
    else:
        ctx['srcname'] = os.path.normpath(ctx['url'])
        ctx['srcname'] = os.path.basename(ctx['srcname'])

    # set workspace prefix
    if not args.prefix:
        try:
            args.prefix = os.environ['MMPACK_PREFIX']
        except KeyError:
            pass
    if args.prefix:
        Workspace().prefix = args.prefix

    return ctx


def main():
    'entry point to create a mmpack package'
    ctx = parse_options(sys.argv[1:])

    package = SrcPackage(name=ctx['srcname'], url=ctx['url'],
                         tag=ctx['tag'])
    src_pkg = package.create_source_archive()

    package.load_specfile()
    package.parse_specfile()

    package.local_install(src_pkg, ctx['skip_tests'])
    package.ventilate()
    package.generate_binary_packages()


if __name__ == '__main__':
    main()
