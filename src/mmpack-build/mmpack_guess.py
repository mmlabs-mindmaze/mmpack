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


from .common import find_license, shell, yaml_load, yaml_serialize
from .errors import ShellException
from .yaml_dumper import MMPackDumper


UNKNOWN = 'unknown'  # default return value if guessing failed


# disable for the whole module
# pylint: disable=broad-except


def add_parser_args(parser: ArgumentParser):
    """Enrich supplied argument parser with guess related arguments"""
    subparsers = parser.add_subparsers(dest='guess_subcmd', required=False)

    subparsers.add_parser('create-specs',
                          help='create mmpack specs for current project')

    prov_parser = subparsers.add_parser('update-provides',
                          help='update provides specs from a project build')
    prov_parser.add_argument('project_builddir',
                          help='path to a mmpack build of the current project')


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


_PROVIDES_REGEX = re.compile(r'([^.]+).(pyobjects|symbols).gz')


def _update_provide_spec(build_provide: str, ptype: str, pkg: str):
    provide_spec_filename = f'mmpack/{pkg}.provides'
    try:
        specs = yaml_serialize(provide_spec_filename)
    except FileNotFoundError:
        specs = {}

    typed_specs = specs.setdefault(ptype, {})
    provides = yaml_serialize(build_provide)


def update_provides(proj_builddir: str):
    metadir = 'var/lib/mmpack/metadata'
    for metafile in glob.glob(f'{proj_builddir}/staging/*/{metadir}/*'):
        base = os.path.basename(metafile)
        match = _PROVIDES_REGEX.fullmatch(base)
        if not match:
            continue


def main(args):
    """
    entry point to guess a mmpack package specs
    """
    if args.guess_subcmd in (None, 'create-specs'):
        guess_specs()
    elif args.guess_subcmd == 'update-provides':
        update_provides(args.project_builddir)
