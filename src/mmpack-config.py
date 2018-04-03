#!/usr/bin/env python3
'''
TODOC
'''

import os
import sys

import yaml

from pprint import pprint
from settings import CONFIG_PATH


def usage(progname):
    'show usage'
    print('usage {}'.format(progname))


def show(argv):
    'dump whole configuration'
    try:
        config = yaml.load(open(CONFIG_PATH, 'rb').read())
    except IOError:
        # could not find config file
        # create one and return it
        config = {'This is a config placeholder': None,
                  'remote': ["fill with mmpack server:",
                             "http://server1:42/custom_url"]}
        with open(CONFIG_PATH, 'w+') as outfile:
            yaml.dump(config, outfile, default_flow_style=False)
    except yaml.parser.ParserError:
        raise  # malformed config file

    print(yaml.dump(config))


def edit(argv):
    'edit config value'
    print('edit')
    pprint(argv)


def options(argv):
    'parse options'
    if argv[1] == 'show':
        return show(argv[2:])
    elif argv[1] == 'edit':
        return edit(argv[2:])

    raise IndexError


def main():
    'TODOC'
    try:
        return options(sys.argv)
    except IndexError:
        return usage(sys.argv[0])


if __name__ == '__main__':
    main()
