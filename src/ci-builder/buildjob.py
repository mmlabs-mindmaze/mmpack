# @mindmaze_header@
"""
Representation of a build job
"""

import os
import shutil
from glob import glob
from tempfile import mkdtemp

from mmpack_build.common import yaml_load, yaml_serialize
from mmpack_build.source_tarball import SourceTarball



class BuildJob:
    # pylint: disable=too-many-instance-attributes
    """
    Class representing a build to be performed
    """
    def __init__(self, project: str, url: str, refspec: str, **kwargs):
        self.project = project
        self.fetch_refspec = refspec
        self.url = url
        self.ref = refspec
        self.srctar = ''
        self.do_upload = True
        self.pkgdir = ''
        self.build_id = ''
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
        self.build_id = os.path.basename(self.pkgdir)

        # create source archive
        srctarball = SourceTarball(method='git',
                                   outdir=self.pkgdir,
                                   path_url=self.url,
                                   tag=self.fetch_refspec)

        self.srctar = srctarball.srctar
        return bool(self.srctar)

    def __repr__(self):
        desc = 'project {} commit {}'.format(self.project, self.ref)
        if self.build_id:
            desc += ' (job id {})'.format(self.build_id)
        return desc

    def notify_result(self, success: bool, message: str = None):
        pass

    def merge_manifests(self) -> str:
        """
        find all mmpack manifest of a folder, and create an aggregated
        version of them in the same folder

        Return: the path to the aggregated manifest.
        """
        common_keys = ('name', 'source', 'version')

        merged = {}
        for manifest_file in glob(self.pkgdir + '/*.mmpack-manifest'):
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

        filename = '{}/{}_{}.mmpack-manifest'.format(self.pkgdir,
                                                     merged['name'],
                                                     merged['version'])
        yaml_serialize(merged, filename)
        return filename
