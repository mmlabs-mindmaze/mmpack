# @mindmaze_header@
"""
Create mmpack package source specified in argument. If no argument is provided,
look through the tree for a mmpack folder, and use the containing folder as
root directory.
"""
from argparse import ArgumentParser, RawDescriptionHelpFormatter

from . source_tarball import SourceTarball


CMD = 'mksource'


def add_mksource_parser_argument(parser: ArgumentParser):
    """
    make parser given in argument understand mksource subcommand option and
    arguments
    """
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--git', dest='method',
                       action='store_const', const='git',
                       help='interpret argument as url/path to git repository')
    group.add_argument('--path', dest='method',
                       action='store_const', const='path',
                       help='interpret argument as path to folder with '
                            'a mmpack dir containing specs')
    group.add_argument('--src', dest='method',
                       action='store_const', const='tar',
                       help='interpret argument as source tarball')
    group.add_argument('--mmpack-src', dest='method',
                       action='store_const', const='srcpkg',
                       help='interpret argument as mmpack source tarball')
    parser.add_argument('-t', '--tag',
                        action='store', dest='tag', type=str,
                        help='project tag')
    parser.add_argument('path_or_url', nargs='?',
                        help='path or url to project providing mmpack package')


def parse_options(argv):
    """
    parse and check options
    """
    parser = ArgumentParser(description=__doc__,
                            prog='mmpack-build ' + CMD,
                            formatter_class=RawDescriptionHelpFormatter)
    add_mksource_parser_argument(parser)
    return parser.parse_args(argv)


def main(argv):
    """
    entry point to create a mmpack package
    """
    args = parse_options(argv[1:])
    srctarball = SourceTarball(args.method, args.path_or_url, args.tag)
    for prj in srctarball.iter_mmpack_srcs():
        print('{} {} {}'.format(prj.name, prj.version, prj.tarball))
