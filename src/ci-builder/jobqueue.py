# @mindmaze_header@
"""
Source providing the adding and executing of build jobs
"""

from queue import SimpleQueue
from threading import Thread
from typing import List

from repo import Repo

from builder import Builder
from buildjob import BuildJob
from common import log_info, log_error


class JobQueue:
    """
    Class to execute job asynchronously
    """
    def __init__(self, repos: List[Repo], builders: List[Builder]):
        self.queue = SimpleQueue()
        self.repos = repos
        self.builders = builders
        self.thread = None

    def process_job(self, job: BuildJob):
        # Generate mmpack source
        log_info('making source package for {}'.format(job))
        done = job.make_srcpkg()
        if not done:
            log_info('No mmpack packaging, build cancelled')
            return
        log_info('Done')

        # Generate mmpack binary package on all builders
        feedback_msgs = []
        success = True
        for builder in self.builders:
            log_info('building {} on {}'.format(job, builder))
            try:
                builder.build(job)
                msg = 'build on {} succeed: {}'.format(builder, str(exception))
                log_info('done')
            except Exception as exception:  # pylint: disable=broad-except
                msg = 'build on {} failed: {}'.format(builder, str(exception))
                log_error('Failed: ' + str(exception))
                success = False
            feedback_msgs.append(msg)

        if not success:
            log_info('Packages update failed')
            job.notify_result(False, '\n'.join(feedback_msgs))
            return

        if not job.do_upload:
            log_info('Packages upload skipped')
            job.notify_result(True, 'Packages upload skipped')
            return

        try:
            # Update repositories
            manifest = job.merge_manifests()
            for repo in self.repos:
                repo.try_handle_upload(manifest, remove_upload=False)
                log_info('Arch {} uploaded on {}'.format(repo.arch, repo.repo_dir))

        except Exception as exception:  # pylint: disable=broad-except
            job.notify_result(True, str(exception))

        job.notify_result(True)

    @staticmethod
    def _process_incoming(job_queue: JobQueue):
        queue = job_queue.queue

        log_info('Job queue started')
        while True:
            job = queue.get()
            if not job:
                break

            job_queue.process_job(job)
            queue.task_done()
        log_info('Job queue stopped')

    def start(self):
        """
        Start asynchronous processing of incoming build jobs
        """
        self.thread = Thread(target=self._process_incoming, args=(self))
        self.thread.start()

    def stop(self):
        """
        Stop asynchronous processing of incoming build jobs
        """
        self.queue.put(None)
        self.thread.join()

    def add_job(self, job: BuildJob):
        """
        Add a job in the queue of processing
        """
        self.queue.put(job)
