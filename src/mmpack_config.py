# @mindmaze_header@
'''
TODOC
'''

import sys
from pprint import pprint
import yaml
from common import yaml_load
from workspace import Workspace


def usage(progname):
    'show usage'
    print('usage {}'.format(progname))


def show():
    'dump whole configuration'
    wrk = Workspace()
    try:
        config = yaml_load(wrk.config)
    except IOError:
        # could not find config file
        # create one and return it
        config = {'This is a config placeholder': None,
                  'remote': ["fill with mmpack server:",
                             "http://server1:42/custom_url"]}
        with open(wrk.config, 'w+') as outfile:
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
        return show()
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
