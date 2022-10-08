# [@]mmpack_header[@]
"""
tool to analyze set of python modules
"""
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath

from astroid import MANAGER as astroid_manager

from .depends import run_depends, __doc__ as depends_doc
from .dispatch import run_dispatch, __doc__ as dispatch_doc
from .provides import run_provides, __doc__ as provides_doc


def parse_options():
    """
    parse options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('--site-path', dest='site_paths', type=str, nargs='?',
                        action='append', default=[],
                        help='path of python site-packages or folder '
                        'containing python package')
    subparsers = parser.add_subparsers(dest='command', required=True)

    cmd_parser = subparsers.add_parser('depends', help=depends_doc)
    cmd_parser.add_argument('infile', type=str, nargs='?')

    cmd_parser = subparsers.add_parser('provides', help=provides_doc)
    cmd_parser.add_argument('infile', type=str, nargs='?')

    cmd_parser = subparsers.add_parser('dispatch', help=dispatch_doc)
    cmd_parser.add_argument('infile', type=str, nargs='?')

    return parser.parse_args()


def main():
    """
    python_depends utility entry point
    """
    options = parse_options()

    # If site path folder is specified, add it to sys.path so astroid resolve
    # the imports properly
    for sitedir in options.site_paths:
        sys.path.insert(0, abspath(sitedir))

    astroid_manager.always_load_extensions = True

    with open(options.infile) as input_stream:
        input_files = [f.strip() for f in input_stream]

    if options.command == 'depends':
        run_depends(input_files, options.site_paths)
    elif options.command == 'provides':
        run_provides(input_files)
    elif options.command == 'dispatch':
        run_dispatch(input_files)


if __name__ == '__main__':
    main()
