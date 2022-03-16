# @mindmaze_header@
"""
Create mmpack package source specified in argument. If no argument is provided,
look through the tree for a mmpack folder, and use the containing folder as
root directory.
"""
import sys
from argparse import ArgumentParser, Namespace
from typing import Iterator

from . errors import MMPackBuildError
from . source_tarball import SourceTarball
from . src_package import SrcPackage


def add_parser_args(parser: ArgumentParser):
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
    parser.add_argument('--multiproject-only-modified',
                        action='store_true', dest='only_modified',
                        help='indicate that the only projects to build are '
                             'those modified by the git commit')
    parser.add_argument('--update-version-from-vcs',
                        action='store_true', default=False,
                        help='update version from commits since version tag')


def main(args):
    """
    entry point to create a mmpack package
    """
    kwargs = {
        'build_only_modified': args.only_modified,
        'version_from_vcs': args.update_version_from_vcs,
    }

    try:
        srctarball = SourceTarball(args.method, args.path_or_url, args.tag,
                                   **kwargs)
        for prj in srctarball.iter_mmpack_srcs():
            print('{} {} {}'.format(prj.name, prj.version, prj.tarball))
    except MMPackBuildError as error:
        print(f'Source tarball generation failed: {error}', file=sys.stderr)
        return 1

    return 0


def src_pkgs_in_prj_repo(args: Namespace) -> Iterator[SrcPackage]:
    """Iterate over all source package meant to be built

    Args:
        args: the parsed options of the mksource arguments.

    Return:
        A iterator of source package
    """
    srctarball = SourceTarball(args.method, args.path_or_url, args.tag,
                               build_only_modified=args.only_modified,
                               version_from_vcs=args.update_version_from_vcs)
    for prj in srctarball.iter_mmpack_srcs():
        yield SrcPackage(prj.tarball, srctarball.tag, prj.srcdir)
