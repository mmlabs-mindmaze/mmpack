# @mindmaze_header@
'''
Guess mmpack specfile from the repository

Usage:

mmpack guess-specs

Expects to be called from the root of the git repository.

Will try to guess mmpack specs from the current repository and print them to
stdout.
'''

import glob
import os
import re
from argparse import ArgumentParser
from typing import List
import yaml


from .common import find_license, shell
from .errors import ShellException
from .yaml_dumper import MMPackDumper


UNKNOWN = 'unknown'  # default return value if guessing failed


# disable for the whole module
# pylint: disable=broad-except


_SUBCMDS = [
    'create-specs',
]


# pylint: disable=unused-argument
def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with guess related arguments"""
    parser.add_argument('guess_subcmd', default=_SUBCMDS[0], nargs='?',
                        choices=_SUBCMDS)


def _sshell(cmd) -> str:
    """
    helper: silent shell call
    """
    try:
        return shell(cmd, log=False, log_stderr=False).strip()
    except ShellException:
        return UNKNOWN


def guess_url() -> str:
    """
    Guess specs url field
    (git only) return origin fetch url
    Assume there is a remote named "origin"
    """
    try:
        remote = _sshell('git remote show -n origin')
        for line in remote.split('\n'):
            line = line.strip()
            if line.startswith('Fetch URL:'):
                return line.split()[-1]
        return remote
    except Exception:
        return UNKNOWN


def guess_name() -> str:
    """
    Guess specs name field
    (git only) return base part of the url
    """
    try:
        url = guess_url()
        name = url.split('/')[-1]
        if name.endswith('.git'):
            name = name[:-len('.git')]
        return name
    except Exception:
        return UNKNOWN


def guess_version() -> str:
    """
    Guess specs version field
    (git only) return value based on last tag'
    """
    try:
        return _sshell(['git', 'describe', '--tags', '--abbrev=0'])
    except Exception:
        return UNKNOWN


def guess_maintainer() -> str:
    """
    Guess specs maintainer field
    (git only) return self
    """
    try:
        return '{0} <{1}>'.format(_sshell('git config user.name'),
                                  _sshell('git config user.email'))
    except Exception:
        return UNKNOWN


def _guess_description(readme: str) -> str:
    # match the first paragraph delimited by an empty line
    # and not starting with '#', '[', or '='
    # prevents markdown titles, and the badges links sometimes put on top
    # expect at least 5 characters
    first_paragraph = re.compile(r'\n\n([^[#=]{5,})\n\n')
    return first_paragraph.search(readme)[0]


def guess_description() -> str:
    """
    guess project description from README
    assuming markdown, and getting 1st parapgraph
    """
    try:
        readme = open(glob.glob('[Rr][Ee][Aa][Dd][Mm][Ee]*')[0]).read()
        return _guess_description(readme)
    except Exception:
        return UNKNOWN


def guess_copyright() -> str:
    """
    guess project copyright

    Assume a "src" folder, take the first file it can find inside
    and return the first line containing the term "copyright" from its
    beginning
    """
    is_copyright = re.compile(r'copyright', re.IGNORECASE)
    ntries = 10
    try:
        for src in glob.glob('src/**', recursive=True):
            if os.path.isfile(src):
                header = open(src).readlines()[0:10]
                for line in header:
                    if is_copyright.search(line):
                        return line
                if ntries > 0:
                    ntries -= 1
                    continue
                break
        return UNKNOWN
    except Exception:
        return UNKNOWN


def guess_licenses() -> List[str]:
    """
    guess project license from a license file (case insensitive)
    Assuming a single license file at the top of the tree
    """
    license_file = find_license()
    return [license_file] if license_file else [UNKNOWN]


def guess_specs():
    """
    guess mmpack specs and print to stdout
    """
    specs = {
        'name': guess_name(),
        'version': guess_version(),
        'maintainer': guess_maintainer(),
        'url': guess_url(),
        'description': guess_description(),
        'copyright': guess_copyright(),
        'licenses': guess_licenses(),
    }
    specs_str = yaml.dump(specs, default_flow_style=False,
                          allow_unicode=True, indent=4,
                          Dumper=MMPackDumper)
    for line in specs_str.split('\n'):
        if line:  # hide empty lines
            print(line)


def main(args):
    """
    entry point to guess a mmpack package specs
    """
    if args.guess_subcmd == 'create-specs':
        guess_specs()
