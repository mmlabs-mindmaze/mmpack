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

import paramiko

from mmpack_build.source_tarball import SourceTarball
from mmpack_build.common import yaml_load, yaml_serialize

CURR_DIR = os.path.abspath(os.path.dirname(__file__))
ABS_REPOSCRIPT_DIR = os.path.abspath(os.path.join(CURR_DIR, '../..'))
sys.path.insert(0, ABS_REPOSCRIPT_DIR)

from repo import Repo  # noqa

import gerrit  # noqa


# configuration global variable
CONFIG_FILEPATH = '/etc/gerrit-mmpack-build/config.yaml'
CONFIG = None
BUILDER_LIST = []
REPO_LIST = []

# repository variables
REPOSITORY_ROOT_PATH = '/data/build/mmpack/unstable'

# logging
logger = None

# timeout values
CONNECT_TIMEOUT = 300  # 5 minutes
TIMEOUT = 3600  # 1 hour, this is a hard stop to any command run on a slave


def init_logger():
    """
    Init logger to print to *log_file*.

    log file is read from the config file 'log-file' key, and defaults
    to '/var/log/gerrit-mmpack-build.log'

    Should be called after loading the CONFIG

    Files rotate every day, and are kept for a month
    """
    global logger  # pylint: disable=global-statement

    log_file = CONFIG.get('log-file', '/var/log/gerrit-mmpack-build.log')
    log_handler = logging.handlers.TimedRotatingFileHandler(log_file,
                                                            when='D',
                                                            backupCount=30)

    formatter = logging.Formatter("%(asctime)s: %(levelname)s: %(message)s",
                                  "%Y-%m-%d %H:%M:%S")
    log_handler.setFormatter(formatter)

    logger = logging.getLogger('gerrit-mmpack-build')
    logger.addHandler(log_handler)
    logger.setLevel(logging.INFO)


def isdir(sftp_client, path):
    """
    test whether remote path is dir or not
    """
    try:
        return stat.S_ISDIR(sftp_client.stat(path).st_mode)
    except IOError:
        return False


class SSH:
    """
    helper class used to execute shell commands on ssh
    """
    # pylint: disable=too-many-arguments
    def __init__(self, name, hostname, username,
                 port='22', keyfile=None, password=None):
        self.name = name
        self.hostname = hostname
        self.username = username
        self.port = int(port)
        self.keyfile = keyfile
        self.password = password

    def __repr__(self):
        return 'SSH ({}): {}@{}:{:d}' \
               .format(self.name, self.username, self.hostname, self.port)

    def _make_client(self):
        """
        return a connected ssh client
        authentication method used:
          1 - pubkey if self.keyfile not None
          2 - password if self.password not None
        If self.password is not None AND self.keyfile is not None,
        self.password is interpreted as passphrase of keyfile
        """
        client = paramiko.SSHClient()
        client.load_system_host_keys()
        client.set_missing_host_key_policy(paramiko.WarningPolicy())
        client.connect(self.hostname,
                       username=self.username,
                       port=self.port,
                       password=self.password,
                       key_filename=self.keyfile,
                       look_for_keys=False,
                       timeout=CONNECT_TIMEOUT)
        return client

    def exec(self, command):
        """
        connect to host, and executes given command
        """
        client = None
        try:
            client = self._make_client()
            stdin, stdout, stderr = client.exec_command(command,
                                                        timeout=TIMEOUT)

            stdin.close()
            channel = stdout.channel  # shared channel for stdout/stderr/stdin
            while not channel.exit_status_ready():
                # flush stdout and stderr while the remote program has not
                # returned. Read by blocks of max 16KB.
                if channel.recv_ready():
                    stdout.channel.recv(1 << 14)
                if channel.recv_stderr_ready():
                    stderr.channel.recv_stderr(1 << 14)
                time.sleep(0.1)  # sleep 100 ms

            stdout.close()
            stderr.close()
            ret = channel.recv_exit_status()
        finally:
            if client:
                client.close()
        if ret:
            errmsg = 'SSH error executing {}'.format(command)
            logger.error(errmsg)
            raise Exception(errmsg)

    def _rec_download(self, sftp_client, remote_dir_path, local_dir_path):
        os.makedirs(local_dir_path, exist_ok=True)
        for filename in sftp_client.listdir(remote_dir_path):
            remote_path = os.path.join(remote_dir_path, filename)
            local_path = os.path.join(local_dir_path, filename)
            if stat.S_ISDIR(sftp_client.stat(remote_path).st_mode):
                os.makedirs(local_path, exist_ok=True)
                self._rec_download(sftp_client, remote_path, local_path)
            else:
                sftp_client.get(remote_path, local_path)

    def get(self, remote_dir_path, local_dir_path):
        """
        Copy a remote file to given local path

        Expects both remote and local arguments to be directories, not files.
        """
        client = None
        try:
            client = self._make_client()
            sftp_client = client.open_sftp()
            self._rec_download(sftp_client, remote_dir_path, local_dir_path)
        except Exception as e:  # pylint: disable=broad-except
            errmsg = 'SSH error retrieving {} from {} into {}' \
                     .format(remote_dir_path, self, local_dir_path)
            errmsg += 'Error: ' + str(e)
            logger.error(errmsg)
        finally:
            if client:
                client.close()

    def put(self, local_file_path, remote_path):
        """
        Copy a local file to given remote path
        """
        client = None
        try:
            client = self._make_client()
            sftp_client = client.open_sftp()
            sftp_client.put(local_file_path, remote_path, confirm=False)
        except Exception as e:  # pylint: disable=broad-except
            errmsg = 'SSH error sending {} to {} in {}\n' \
                     .format(local_file_path, self, remote_path)
            errmsg += 'Error: ' + str(e)
            logger.error(errmsg)
        finally:
            if client:
                client.close()


def _trigger_build(event):
    """
    test whether an event is a merge event, or a manual trigger
    """
    try:
        do_build = (event['type'] == 'change-merged'
                    or (event['type'] == 'comment-added'
                        and ('MMPACK_BUILD' in event['comment']
                             or 'MMPACK_UPLOAD_BUILD' in event['comment'])))
        do_upload = (event['type'] == 'change-merged'
                     or (event['type'] == 'comment-added'
                         and 'MMPACK_UPLOAD_BUILD' in event['comment']))

        return do_build, do_upload
    except KeyError:
        return False


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
        logger.error(errmsg)
        return -1


def merge_manifests(pkgdir: str):
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
            logger.error('Malformed event')
            return 0

        logger.info('building {} branch {}'.format(project, branch))

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
            logger.info('No mmpack packaging, build cancelled')
            return 0

        for node in BUILDER_LIST:
            logger.info('building {} on {} using remote folder {}'
                        .format(project, node, workdir))
            if build_packages(node, workdir, tmpdir, srctarball.srctar):
                return -1
            logger.info('building {} done on {}'.format(project, node))
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
        logger.error(errmsg)


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
            logger.error('ignoring exception {}'.format(str(err)))
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == '__main__':
    main()
