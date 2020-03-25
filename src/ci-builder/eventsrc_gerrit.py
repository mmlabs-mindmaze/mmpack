# @mindmaze_header@
"""
Source build event using Gerrit
"""

from typing import Dict

from buildjob import BuildJob
from common import log_error
from eventsrc import EventSource
from jobqueue import JobQueue
from gerrit import Gerrit


class GerritBuildJob(BuildJob):
    def __init__(self, gerrit: Gerrit, project: str, change: str):
        git_url = 'ssh://{}@{}:{:d}/{}'.format(gerrit.username,
                                               gerrit.hostname,
                                               int(gerrit.port),
                                               project)
        opts = {'git_ssh_cmd': 'ssh -i ' + gerrit.keyfile}

        super().__init__(project=project,
                         url=git_url,
                         refspec=change,
                         clone_opts=opts)
        self.gerrit_instance = gerrit
        self.change = change

    def notify_result(self, success: bool, message: str = None):
        self.gerrit_instance.review(self.project, self.change)


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


class GerritEventSource(EventSource):
    def __init__(self, jobqueue: JobQueue, config=Dict[str, str]):
        """
        Initialize a Gerrit based event source

        Args:
            @jobqueue: jobqueue to which job must be added
            @config: dictionary configuring the connection to gerrit. Allowed
                keys are 'hostname', 'username', 'port' and 'keyfile'
        """
        super().__init__(jobqueue)
        self.gerrit_instance = Gerrit(**config)

    def _handle_gerrit_event(self, event: dict):
        do_build, do_upload = _trigger_build(event)
        if not do_build:
            return

        job = GerritBuildJob(self.gerrit_instance,
                             event['change']['project'],
                             event['patchSet']['revision'])
        job.do_upload = do_upload
        self.add_job(job)

    def run(self):
        self.gerrit_instance.startWatching()

        while True:
            try:
                event = self.gerrit_instance.getEvent()
            except Exception as err:  # pylint: disable=broad-except
                # an error occurred, but NOT one involving package
                # generation just let slide, it may be caused by a hiccup
                # in the infrastructure.
                log_error('ignoring exception {}'.format(str(err)))
                continue

            self._handle_gerrit_event(event)
