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
CONFIG_FILEPATH = '/etc/mmpack-ci-builder/config.yaml'
CONFIG = None
BUILDER_LIST = []
REPO_LIST = []
def build_packages(node, workdir, tmpdir, srctar):
    """
    build project on given branch

    Args:
        node: the build slave to work on
        workdir: the *remote* working directory will be flushed when leaving
        tmpdir: *local* directory that will receive the packages (tmpdir is
            not cleansed by this function)
        srctar: source tarball to compile

    Returns:
        0 in case of success, -1 otherwise
    """
    remote_srctar = os.path.join(workdir, os.path.basename(srctar))
    try:
        # upload source package to build slave node
        node.exec('mkdir -p ' + workdir)
        node.put(srctar, remote_srctar)
        cmd = 'mmpack-build-slave.sh {} {}'.format(workdir, remote_srctar)
        node.exec(cmd)

        node.get(workdir + '/mmpack-packages', tmpdir)  # retrieve packages
        node.exec('rm -rf ' + workdir)  # wipe remote tmp directory

        return 0
    except Exception as e:  # pylint: disable=broad-except
        errmsg = 'Failed to build package {} on node {}. Exception: {}' \
                 .format(workdir, node, str(e))
        log_error(errmsg)
        return -1


def merge_manifests(pkgdir: str) -> str:
    """
    find all mmpack manifest of a folder, and create an aggregated
    version of them in the same folder

    Return: the path to the aggregated manifest.
    """
    common_keys = ('name', 'source', 'version')

    merged = {}
    for manifest_file in glob(pkgdir + '/*.mmpack-manifest'):
        elt_data = yaml_load(manifest_file)
        if not merged:
            merged = elt_data

        # Check consistency between source, name and source version
        merged_common = {k: v for k, v in merged.items() if k in common_keys}
        elt_common = {k: v for k, v in elt_data.items() if k in common_keys}
        if merged_common != elt_common:
            raise RuntimeError('merging inconsistent manifest')

        # merged list of binary packages for each architecture
        merged['binpkgs'].update(elt_data['binpkgs'])

    filename = '{}/{}_{}.mmpack-manifest'.format(pkgdir,
                                                 merged['name'],
                                                 merged['version'])
    yaml_serialize(merged, filename)
    return filename


def process_event(event, tmpdir):
    """
    filter and processes event
    """
    do_build, do_upload = _trigger_build(event)
    if do_build:
        # extract event information
        try:
            project = event['change']['project']
            if do_upload:  # use branch name
                branch = event['change']['branch']
            else:  # use git sha1
                branch = event['patchSet']['revision']
        except KeyError:
            # it is impossible to notify gerrit in such a case
            log_error('Malformed event')
            return 0

        log_info('building {} branch {}'.format(project, branch))

        workdir = os.path.join('/tmp/mmpack', project, branch)

        # create source archive
        project_git_url = 'ssh://{}@{}:{:d}/{}' \
                          .format(CONFIG['gerrit']['username'],
                                  CONFIG['gerrit']['hostname'],
                                  int(CONFIG['gerrit']['port']),
                                  project)
        gitclone_ssh_cmd = 'ssh -i ' + CONFIG['gerrit']['keyfile']
        srctarball = SourceTarball(method='git',
                                   path_url=project_git_url,
                                   tag=branch,
                                   outdir=tmpdir,
                                   git_ssh_cmd=gitclone_ssh_cmd)

        # If project is not packaged with mmpack, just skip
        if not srctarball.srctar:
            log_info('No mmpack packaging, build cancelled')
            return 0

        for node in BUILDER_LIST:
            log_info('building {} on {} using remote folder {}'
                     .format(project, node, workdir))
            if build_packages(node, workdir, tmpdir, srctarball.srctar):
                return -1
            log_info('building {} done on {}'.format(project, node))
    # At this point, all builders have succeeded
    # upload packages only if trigger is a merge action
    if not do_upload:
        return 0

    manifest_file = merge_manifests(tmpdir)
    for repo in REPO_LIST:
        repo.try_handle_upload(manifest_file, remove_upload=False)

    return 0


def gerrit_notify_error(g, event):
    """
    logs an error into gerrit
    This function is silent on error
    """
    try:
        project = event['change']['project']
        changeid = event['patchSet']['revision']
        g.review(project, changeid, 'mmpack-build pkg-create FAILED')
    except Exception as e:  # pylint: disable=broad-except
        errmsg = 'Failed to notify gerrit of build error.\n'
        errmsg += 'Error: ' + str(e)
        log_error(errmsg)


def load_config(filename):
    """
    load configuration file and return it as a dict
    """
    # pylint: disable=global-statement
    global CONFIG
    # pylint: disable=global-variable-not-assigned
    global BUILDER_LIST
    # pylint: disable=global-variable-not-assigned
    global REPO_LIST

    CONFIG = yaml_load(filename)
    for name, node in CONFIG['builders'].items():
        BUILDER_LIST.append(SSH(name=name, **node))
    for _, node in CONFIG['repositories'].items():
        REPO_LIST.append(Repo(repo=node['path'],
                              architecture=node['architecture']))


def main():
    """
    main function
    """
    load_config(CONFIG_FILEPATH)
    init_logger()
    g = gerrit.Gerrit(hostname=CONFIG['gerrit']['hostname'],
                      username=CONFIG['gerrit']['username'],
                      port=int(CONFIG['gerrit']['port']),
                      keyfile=CONFIG['gerrit']['keyfile'])

    g.startWatching()

    while True:
        tmpdir = mkdtemp(prefix='mmpack')
        try:
            event = g.getEvent()
            if process_event(event, tmpdir) < 0:
                gerrit_notify_error(g, event)
        except Exception as err:  # pylint: disable=broad-except
            # an error occurred, but NOT one involving package generation
            # just let slide, it may be caused by a hiccup in the
            # infrastructure.
            log_error('ignoring exception {}'.format(str(err)))
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == '__main__':
    main()
