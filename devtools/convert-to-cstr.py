#!/usr/bin/python3
# @mindmaze_header@

from argparse import ArgumentParser, RawDescriptionHelpFormatter
from functools import partial
import sys


def parse_opts():
    parser = ArgumentParser(prog='file-to-cstr',
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('-o', '--output-file', help='output file',
                        action='store', dest='output', nargs='?', default='-')
    parser.add_argument('input', nargs='?',
                        help='input file to convert in C-string init value')
    return parser.parse_args()


def main():
    opts = parse_opts()

    if opts.output == '-':
        outfile = sys.stdout
    else:
        outfile = open(opts.output, 'w', newline='\n')

    with open(opts.input, 'rb') as infile:
        for chunk in iter(partial(infile.read, 16), b''):
            outfile.write(' '.join([f'0x{b:02x},' for b in chunk]) + '\n')

    if outfile != sys.stdout:
        outfile.close()


if __name__ == "__main__":
    main()
