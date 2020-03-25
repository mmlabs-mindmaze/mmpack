# @mindmaze_header@
"""
Source for handling scanning change in repository
"""

from buildjob import BuildJob
from jobqueue import JobQueue


class EventSource:
    def __init__(self, jobqueue: JobQueue):
        self.jobqueue = jobqueue

    def add_job(self, job: BuildJob):
        self.jobqueue.add_job(job)

    def run(self):
        pass
