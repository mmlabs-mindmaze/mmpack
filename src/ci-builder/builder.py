# @mindmaze_header@
"""
Access to the build servers (build slaves)
"""

import os

import paramiko

from buildjob import BuildJob
from common import log_info, log_error


# timeout values
CONNECT_TIMEOUT = 300  # 5 minutes
TIMEOUT = 3600  # 1 hour, this is a hard stop to any command run on a slave


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
            log_error(errmsg)
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
            log_error(errmsg)
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
            log_error(errmsg)
        finally:
            if client:
                client.close()


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


class Builder:
    """
    Class representing an access to a build server

    Currently only SSH connection to build server is supported
    """
    def __init__(self, name, hostname, username,
                 port='22', keyfile=None, password=None):
        self.base_workdir = '/tmp/mmpack-builds'
        self.node = SSH(name, hostname, username, port, keyfile, password)

    def __repr__(self):
        return repr(self.node)

    def build(self, job_id, srctar, pkgdir) -> bool:
        workdir = os.path.join(self.base_workdir, job_id)
        retval = build_package(self.node, workdir, srctar)
        return True if retval == 0 else False
