# @mindmaze_header@
'''
TODOC
'''

import sys


def usage(progname):
    'show usage'
    print('usage {}'.format(progname))


def main():
    'TODOC'
    try:
        raise IndexError
    except IndexError:
        return usage(sys.argv[0])


if __name__ == '__main__':
    main()
