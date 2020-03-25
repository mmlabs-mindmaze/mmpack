#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation
# pylint: disable=wrong-import-position
"""
Simple script which listens to gerrit for merge events
to create the updated package of the project.
Can also be triggered b y the "MMPACK-BUILD" keyword

The package generated will only be uploaded to the configured repository
on gerrit merge actions to allow users to repeatedly try to build even
invalid packages creations.

Note: this is intended to never fail.
this explains some of the broad excepts, and flags like ignore_errors
"""

# imports
from argparse import ArgumentParser, RawDescriptionHelpFormatter
import logging
import logging.handlers
import os
import shutil
import stat
import sys
import time

from glob import glob
from tempfile import mkdtemp

from mmpack_build.source_tarball import SourceTarball
from mmpack_build.common import yaml_load, yaml_serialize

from common import init_logger

CURR_DIR = os.path.abspath(os.path.dirname(__file__))
ABS_REPOSCRIPT_DIR = os.path.abspath(os.path.join(CURR_DIR, '../..'))
sys.path.insert(0, ABS_REPOSCRIPT_DIR)

from repo import Repo  # noqa


# configuration global variable
DEFAULT_CONFIGPATH = '/etc/mmpack-ci-builder/config.yaml'
DEFAULT_LOGPATH = '/var/log/mmpack-ci-builder.log'


def create_event_source(jobqueue: JobQueue,
                        config: Dict[str, str]) -> EventSource:
    EVENT_SOURCES_CONSTRUCTORS = {
        'gerrit': GerritEventSource,
    }

    srctype = config['type']
    return EVENT_SOURCES_CONSTRUCTORS[src_type](jobqueue, **config)


class CIApplication:
    def __init__(self, filename: str):
        cfg = yaml_load(filename)

        init_logger(cfg.get('logpath', DEFAULT_LOGPATH))

        repos = [Repo(repo=node['path'], architecture=node['architecture'])
                 for node in cfg['repositories'].values()]
        builders = [Builder(name=name, **node)
                    for name, node in cfg['builders'].items()]

        self.jobqueue = JobQueue(repos, builders)
        self.eventsrc = create_event_source(self.jobqueue, cfg['eventsrc'])

    def run(self):
        self.jobqueue.start()
        self.eventsrc.run()


def parse_options(argv: List[str]):
    """
    parse and check options
    """
    parser = ArgumentParser(description=__doc__,
                            formatter_class=RawDescriptionHelpFormatter)

    parser.add_argument('-c', '--config', default=DEFAULT_CONFIGPATH,
                        action='store', dest='configpath', type=str,
                        help='path to configuration file')

    return parser.parse_args(argv)


def main(argv: List[str]):
    """
    main function
    """
    args = parse_args(argv)
    ci_app = CIServer(args.configpath)
    ci_app.run()


if __name__ == '__main__':
    main(sys.argv)
