# @mindmaze_header@

from argcomplete import autocomplete

from .__main__ import cmdline_parser


def main():
    parser = cmdline_parser()
    autocomplete(parser)

if __name__ == "__main__":
    main()
