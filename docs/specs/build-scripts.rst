Build scripts
=============

When building the source package or the binary package from a source package,
it is possible to call several scripts to implement non standard behavior.
Those scripts are located within the folder **mmpack** along with other package
related material and get executed when the associated condition or step is
reached and if the according script exists.

Currently they can only be POSIX shell script.


Package source build scripts
----------------------------


create_srcdir_script
''''''''''''''''''''

condition
`````````
When the mmpack packaging source have been extracted in its build folder. If
the source are from a fully mmpack source package, the hook will not be called.

Arguments
`````````
the method used for getting the source: git, path, tar

environment
```````````
The script execution inherit the environment of the build. In addition, the
following variables are set:

BUILDDIR: directory used to build the package
PATH_URL: origin of the source
VCSDIR: location of the VCS files (only if method is git, subdir of BUILDDIR)

execution dir
`````````````
location of the mmpack source unpacked (subdir of BUILDDIR)


fetch_upstream_script
'''''''''''''''''''''

condition
`````````
If a source-strap file can be found in **mmpack**, when the upstream project
have just been fetched.

Arguments
`````````
the method used for getting the upstream: git, tar

environment
```````````
The script execution inherit the environment of the build. In addition, the
following variables are set:

BUILDDIR: directory used to build the package
URL: origin of the upstream
VCSDIR: location of the VCS files of unpacked upstream (only if is git, subdir of BUILDDIR)

execution dir
`````````````
location of the upstream source have been unpacked (subdir of BUILDDIR)


source_strapped_hook
''''''''''''''''''''

condition
`````````
If a source-strap file can be found in **mmpack**, when the upstream project
have just been fetched, merged with mmpack packaging files and patches, if any
have been applied.

Arguments
`````````
the method used for getting the upstream: git, tar

environment
```````````
The hook inherit the environment of the build. In addition, the following
variables are set:

BUILDDIR: directory used to build the package

execution dir
`````````````
location where the source have been preprared (subdir of BUILDDIR)


Binary package build scripts
----------------------------

local_install
'''''''''''''

condition
`````````
The build script has been called (files are installed locally). The
post_local_install methods of the hooks have not been called yet.

Arguments
`````````
None

environment
```````````
The script execution inherit the environment of the build. In addition, the
following variables are set:

BUILDDIR: directory used to build the project
SPECDIR: location of mmpack spec files

execution dir
`````````````
location of the installed files (subdir of BUILDDIR)

