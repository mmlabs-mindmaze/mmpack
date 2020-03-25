# @mindmaze_header@
"""
Source providing the adding and executing of build jobs
"""

import os
import shutil
from glob import glob
from tempfile import mkdtemp
from threading import Thread

from mmpack_build.source_tarball import SourceTarball
from mmpack_build.common import yaml_load, yaml_serialize

from builder import builder
from common import log_info


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


class BuildJob:
    """
    Class representing a build to be performed
    """
    def __init__(self, project: str, url: str, refspec: str, **kwargs):
        self.project = project
        self.fetch_refspec = refspec
        self.url = ''
        self.ref = refspec
        self.srctar = ''
        self.do_upload = True
        self.pkgdir = ''
        self.job_id = ''
        self.srctar_make_opts = kwargs

    def __del__(self):
        if self.pkgdir:
            shutil.rmtree(self.pkgdir, ignore_errors=True)

    def make_srcpkg(self) -> str:
        """
        Generate the mmpack source package from a git commit

        Return:
            True in case of success, False if the commit does not contain
            any mmpack packaging specs.
        """
        self.pkgdir = mkdtemp(prefix='mmpack')
        self.job_id = os.path.basename(self.pkgdir)

        # create source archive
        srctarball = SourceTarball(method='git',
                                   outdir=self.pkgdir,
                                   url=self.url,
                                   tag=self.fetch_refspec)

        self.srctar = srctarball.srctar
        return True if self.srctar else False

    def __repr__(self):
        desc = 'project {} commit {}'.format(self.project, self.ref)
        if self.job_id:
            desc += ' (job id {})'.format(self.job_id)
        return desc

    def notify_result(self, success: bool, message: str = None):
        pass


class JobQueue:
    """
    Class to execute job asynchronously
    """
    def __init__(self, repos: List[Repo], builders: List[Builder]):
        self.repos = repos
        self.builders = builders
        self.thread = None

    def _process_job(self, job: BuildJob):
        # Generate mmpack source
        log_info('making source package for {}'.format(job))
        done = job.make_srcpkg()
        if not done:
            log_info('No mmpack packaging, build cancelled')
            return
        log_info('Done')

        # Generate mmpack binary package on all builders
        for builder in self.builders:
            log_info('building {} on {}'.format(job, builder))
            if not builder.build(self.job_id, job.srctar, self.pkgdir):
                msg = 'build failed on {}. Build cancelled'.format(node)
                log_info(msg)
                job.notify_result(False, msg)
                return
            log_info('done')
        manifest = merge_manifests(job.pkgdir)

        if not job.do_upload:
            log_info('Packages upload skipped')
            job.notify_result(True, 'Packages upload skipped')
            return

        # Update repositories
        for repo in self.repos:
            repo.try_handle_upload(manifest, remove_upload=False)
            log_info('Arch {} uploaded on {}', repo.arch, repo.repo_dir)

        job.notify_result(True)

    @static_method
    def _process_incoming(job_queue: JobQueue):
        queue = job_queue.queue

        log_info('Job queue started')
        while True:
            job = queue.get()
            if not job:
                break

            job_queue._process_job(job)
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

    def add_job(job: BuildJob):
        """
        Add a job in the queue of processing
        """
        self.queue.put(job)
