#!/usr/bin/env python3
# pylint: disable=invalid-name
# pylint: disable=logging-format-interpolation
'''
Simple script which listens to gerrit for merge events
to create the updated package of the project.
Can also be triggered b y the "MMPACK-BUILD" keyword

The package generated will only be uploaded to the configured repository
on gerrit merge actions to allow users to repeatedly try to build even
invalid packages creations.

Note: this is intended to never fail.
this explains some of the broad excepts, and flags like ignore_errors
'''

# imports
from glob import glob
import logging
import logging.handlers
import os
import shutil
from tempfile import mkdtemp
import stat
from subprocess import PIPE, run
import time
import yaml

import paramiko
import gerritlib.gerrit as gerrit


# configuration global variable
CONFIG_FILEPATH = '/etc/gerrit-mmpack-build/config.yaml'
CONFIG = None
BUILDER_LIST = []

# repository variables
REPOSITORY_ROOT_PATH = '/data/build/mmpack/unstable'

# logging
logger = None

# timeout values
CONNECT_TIMEOUT = 300  # 5 minutes
TIMEOUT = 3600  # 1 hour, this is a hard stop to any command run on a slave


def init_logger():
    '''
    Init logger to print to *log_file*.

    log file is read from the config file 'log-file' key, and defaults
    to '/var/log/gerrit-mmpack-build.log'

    Should be called after loading the CONFIG

    Files rotate every day, and are kept for a month
    '''
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
    'test whether remote path is dir or not'
    try:
        return stat.S_ISDIR(sftp_client.stat(path).st_mode)
    except IOError:
        return False


class SSH(object):
    'helper class used to execute shell commands on ssh'
    # pylint: disable=too-many-arguments
    def __init__(self, name, hostname, username,
                 port=22, keyfile=None, password=None):
        self.name = name
        self.hostname = hostname
        self.username = username
        self.port = port
        self.keyfile = keyfile
        self.password = password

    def __repr__(self):
        return 'SSH ({}): {}@{}:{:d}' \
               .format(self.name, self.username, self.hostname, self.port)

    def _make_client(self):
        '''return a connected ssh client
           authentication method used:
             1 - pubkey if self.keyfile not None
             2 - password if self.password not None
           If self.password is not None AND self.keyfile is not None,
           self.password is interpreted as passphrase of keyfile
        '''
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
        'connect to host, and executes given command'
        client = None
        try:
            client = self._make_client()
            stdin, stdout, stderr = client.exec_command(command,
                                                        timeout=TIMEOUT)

            stdin.close()
            channel = stdout.channel  # shared channel for stdout/stderr/stdin
            while not channel.exit_status_ready():
                # flush stdout and stderr while the remote program has not
                # returned. Read by blocs of max 16KB.
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
        '''Copy a remote file to given local path

        Expects both remote and local arguments to be directories, not files.
        '''
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
        'Copy a local file to given remote path'
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

    def _rec_rmdir(self, sftp_client, remote_dir_path):
        for filename in sftp_client.listdir(path=remote_dir_path):
            fullpath = os.path.join(remote_dir_path, filename)
            if isdir(sftp_client, fullpath):
                self._rec_rmdir(sftp_client, fullpath)
            else:
                sftp_client.remove(fullpath)

    def rmdir(self, remote_dir_path):
        'Remove the given folder'
        client = None
        try:
            client = self._make_client()
            sftp_client = client.open_sftp()
            self._rec_rmdir(sftp_client, remote_dir_path)
        finally:
            if client:
                client.close()


def _trigger_build(event):
    'test whether an event is a merge event, or a manual trigger'
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


class ShellException(RuntimeError):
    'custom exception for shell command error'


def shell(cmd):
    'Wrapper for subprocess.run'
    ret = run(cmd, stdout=PIPE, shell=True)
    if ret.returncode == 0:
        return ret.stdout.decode('utf-8')
    else:
        raise ShellException('failed to exec command')


def build_packages(node, workdir, tmpdir):
    '''build project on given branch

    Args:
        node: the build slave to work on
        branch: git branch (accepts tag and sha1)
        workdir: the *remote* working directory
                 will be flushed when leaving
        tmpdir: *local* directory that will receive the packages
                (tmpdir is not cleansed by this function)
    '''
    try:
        # upload source package to build slave node
        node.exec('mkdir -p ' + workdir)
        node.put(tmpdir + '/sources.tar.gz', workdir + '/sources.tar.gz')
        cmd = 'mmpack-build-slave.sh {}'.format(workdir)
        node.exec(cmd)

        node.get(workdir + '/mmpack-packages', tmpdir)  # retrieve packages
        node.rmdir(workdir)  # wipe remote tmp directory

        return 0
    except Exception as e:  # pylint: disable=broad-except
        errmsg = 'Failed to build package {} on node {}. Exception: {}' \
                 .format(workdir, node, str(e))
        logger.error(errmsg)
        return -1


def process_event(event, tmpdir):
    'filter and processes event'
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
            logger.error('Malformed event: ' + event)
            return -1

        logger.info('building {} branch {}'.format(project, branch))

        workdir = os.path.join('/tmp/mmpack', project, branch)

        # create source archive
        project_git_url = 'ssh://{}@{}:{:d}/{}' \
                          .format(CONFIG['gerrit']['username'],
                                  CONFIG['gerrit']['hostname'],
                                  int(CONFIG['gerrit']['port']),
                                  project)
        cmd = 'GIT_SSH_COMMAND="ssh -i {}" ' \
              .format(CONFIG['gerrit']['keyfile'])
        cmd += 'git archive --format=tar.gz --remote={0} {1} > {2}/{3}' \
               .format(project_git_url, branch, tmpdir, 'sources.tar.gz')
        shell(cmd)

        for node in BUILDER_LIST:
            logger.info('building {} on {} using remote folder {}'
                        .format(project, node, workdir))
            if build_packages(node, workdir, tmpdir):
                return -1
            logger.info('building {} done on {}'.format(project, node))
    # At this point, all builders have succeeded
    # upload packages only if trigger is a merge action
    if not do_upload:
        return 0

    for node in BUILDER_LIST:
        # upload packages to repository
        dstdir = os.path.join(CONFIG['repository-root-path'], node.name)
        node.exec('mkdir -p ' + dstdir)
        # Copy binary packages
        for filename in glob(tmpdir + '/*-{}.mpk'.format(node.name)):
            shutil.copy(os.path.join(tmpdir, filename), dstdir)
        # Copy source package
        for filename in glob(tmpdir + '/*-{}_src.*'.format(node.name)):
            shutil.copy(os.path.join(tmpdir, filename), dstdir)

        # update repository index
        repository_root = '{}/{}'.format(CONFIG['repository-root-path'],
                                         node.name)
        cmd = 'mmpack-createrepo {0} {0}'.format(repository_root)
        shell(cmd)

    return 0


def gerrit_notify_error(g, event):
    '''logs an error into gerrit
    This function is silent on error
    '''
    try:
        project = event['change']['project']
        changeid = event['patchSet']['revision']
        g.review(project, changeid, 'mmpack-build pkg-create FAILED')
    except Exception as e:  # pylint: disable=broad-except
        errmsg = 'Failed to notify gerrit of build error.\n'
        errmsg += 'Error: ' + str(e)
        logger.error(errmsg)


def load_config(filename):
    'load configuration file and return it as a dict'
    # pylint: disable=global-statement
    global CONFIG
    # pylint: disable=global-variable-not-assigned
    global BUILDER_LIST

    CONFIG = yaml.load(open(filename, 'rb').read())
    for name, node in CONFIG['builders'].items():
        BUILDER_LIST.append(SSH(name=name, **node))


def main():
    'main function'
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
            logger.error('ignoring exception ' + str(err))
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == '__main__':
    main()
