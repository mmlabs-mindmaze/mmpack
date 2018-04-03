#!/usr/bin/env python3
'''
TODOC
'''

import os
import glob

# ruamel.yaml is a wrapper over the yaml package
# it dumps easier to read (humanly) yaml files
from ruamel.yaml import YAML

from subprocess import run, PIPE
from typing import List

yaml = YAML()


def shell(cmd):
    'wrapper for subprocess.run'
    r = run(cmd, stdout=PIPE, shell=True)
    if r.returncode == 0:
        return r.stdout.decode('utf-8')
    else:
        raise Exception('failed to exec command')


def get_host_pkgmgr():
    'get host package manager'
    import platform
    host_os = platform.dist()[0].lower()
    if host_os == 'debian':
        return 'apt'
    else:
        raise NotImplementedError('Unsupported os: {}'.format(host_os))


def guess_project_version():
    'git only'
    return shell(['git describe', '--tags', '--abbrev=1']).strip()


def guess_project_description():
    'from Readme markdown 1st header'
    README = glob.glob('[Rr][Ee][Aa][Dd][Mm][Ee]*')[0]
    try:
        with open(README, 'r') as f:
            return f.read().split('# ')[1]
    except:
        return 'Could not guess project description'


def guess_project_build_system():
    'TODOC'
    if os.path.exists('configure.ac'):
        return 'autotools'
    elif os.path.exists('CMakeLists.txt'):
        return 'cmake'
    elif os.path.exists('Makefile'):
        return 'makefile'
    else:
        raise Exception('could not guess project build system')


def filetype(filename):
    'get file type'
    # file  --brief --preserve-date <file>
    return os.path.splitext(filename)[1][1:].strip().lower()


def scan_dependences_python(filename):
    'TODO'
    return []


def guess_project_dependencies():
    'foreach file in project, call right parser'
    dep_list = []
    file_list = shell(['git ls-files']).split('\n')
    for filename in file_list:
        ftype = filetype(filename)
        if ftype == 'py':
            dep_list += scan_dependences_python(filename)
        # TODO all others file types

    return dep_list


def guess_project_maintainer():
    'git only'
    try:
        return '{0} <{1}>'.format(shell(['git config user.name']).strip(' \n\r\t'),
                                  shell(['git config user.email']).strip(' \n\r\t'))
    except:
        return 'could not guess project maintainer'


def generate_changelog(from_tag=None):
    'git only'
    if from_tag:
        arc = '{}..HEAD'.format(from_tag)
    else:
        arc = 'HEAD'

    git_cmd = ["git log {0} --format='%cd, %an <%ae>, %s' --date='short'".format(arc)]
    try:
        changelog_raw = shell(git_cmd)
        log_list_dict = []
        for _entry in changelog_raw.split('\n'):
            if not _entry:
                continue
            entry = _entry.split(',')
            log_list_dict.append({'date': entry[0],
                                  'author': entry[1],
                                  'message': ','.join(entry[2:])})
        return log_list_dict
    except:
        return ['failed to generate project changelog']


def mmpack_guess(path=None, host_pkgmgr=None, name=None, version=None):
    '''TODOC
    will raise an exception on failure
    '''
    if not path:
        path = os.getcwd()
    if not host_pkgmgr:
        host_pkgmgr = get_host_pkgmgr()
    if not version:
        version = guess_project_version()

    build_system = guess_project_build_system()

    # guess stuff (will not raise any exception)
    if not name:
        name = os.path.basename(path)
        if not name:
            name = 'unknown'

    build_dep_list = []

    dep_list = guess_project_dependencies()
    maintainer = guess_project_maintainer()
    description = guess_project_description()
    changelog_list = generate_changelog()

    # the project specs as a dict
    project_specs = {
        'name': name,
        'version': version,
        'maintainer': maintainer,
        'description': description,
        'dependencies': dep_list,

        'build': {
            'toolchain': build_system,
            'dep-list': build_dep_list
        }
    }

    # write it all down
    os.makedirs('mmpack', exist_ok=True)
    with open('mmpack/{}.yaml'.format(name), 'w') as f:
        yaml.dump(project_specs, f)
    with open('mmpack/changelog.yaml', 'w') as f:
        yaml.dump(changelog_list, f)


if __name__ == '__main__':
    mmpack_guess(os.getcwd())
