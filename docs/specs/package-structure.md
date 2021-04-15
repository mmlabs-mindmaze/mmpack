# package structure

## file format

tar.zst file with extension .mpk containing:

 * files to install
 * MMPACK/metadata: basic information concerning the package
 * MMPACK/info: YAML description of binary package
 * <metadatadir>/<pkgname>.pkginfo: key/values describing the binary package
 * <metadatadir>/<pkgname>.sha256sums: YAML dictionary of SHA256 hash of
   installed files
 * <metadatadir>/<pkgname>.symbols.gz: YAML description of symbols exported by
   ABI of a shared lib of in the package (if applicable)
 * <metadatadir>/<pkgname>.pyobjects.gz: YAML description of python objects
   (function or class) exported by the package (if applicable)
 * <metadatadir>/<pkgname>.post-install: script to execute (if any) after
   package install
 * <metadatadir>/<pkgname>.pre-uninstall: script to execute (if any) before
   package removal
 * <metadatadir>/<pkgname>.post-uninstall: script to execute (if any) after
   package removal

metadatadir is var/lib/mmpack/metadata.

Regarding post-install, pre-uninstall and post-uninstall files, currently,
only POSIX shell script are allowed. If there is a _real_ need in future
for other type of interpreter, there can be allowed if we add a predepends
field in info file.


## metadata file

key/value file located in the MMPACK folder of the binary package. It it used
to gather basic information regarding the binary package useful before
unpacking the whole package data and helps to localized path to richer files in
the package. It consists of the following fields:

 * metadata-version: version of the format of those metadata (currently 1.0)
 * name: name of the package
 * version: version of the package
 * source: name of the source package
 * srcsha256: SHA256 hash of source package used to build this package
 * sumsha256sums: SHA256 hash of list of installed files (sha256sums file)
 * pkginfo-path: location within package of the richer description (pkginfo file)
 * sumsha-path: location within package of the sha256sums file

This metadata file is not installed in the package upon package installation


## pkginfo file

key/value file located in the metadata dir of mmpack in the prefix named after
the package name: <metadatadir>/<pkgname>.pkginfo. It describes the package and
its relationship with the other packages. It contains the follow fields:

 * name: name of the package
 * version: version of the package
 * source: name of the source package
 * srcsha256: SHA256 hash of source package used to build this package
 * ghost: true or false depending the package is a ghost package
 * description: string describing what the package does/provides
 * depends: list of package specification (new format) indicating the
   dependencies of this package. This field is optional.
 * sysdepends: comma separated list of system package specification indicating
   the system dependencies of this package. The format is of each element is
   specific to the system package manager. This field is optional.


## info file

YAML file located in the MMPACK folder in the binary package that describes the
package and its relationship with the other packages. It contains one entry
named after the package name with the following fields:

 * version: version of the package
 * source: name of the source package
 * sysdepends: comma separated list of system package specification indicating
   the system dependencies of this package. The format is of each element is
   specific to the system package manager. This field is optional.
 * depends: list of package specification indicating the
   dependencies of this package. This field is optional.
 * conflicts: list of package specification that must be
   removed before the package being installed. This field is optional.
 * provides: metapackage that this package provides. This field is optional
 * description: string describing what the package does/provides
 * sumsha256sums: SHA256 hash of list of installed files (MMPACK/sha256sums)
 * srcsha256: SHA256 hash of source package used to build this package
 * ghost: true or false depending the package is a ghost package

## license and copyright files

Raw text files located in a shared dedicated directory (/share/licenses/)
that describe the copyright of the project, along with a copy of the various
licenses used.


## sha256sums file

YAML file located in the mmpack metadata folder in the binary package that list
all installed files by a binary package along with their SHA256 hash values. As
an exception, the sha256sums (although it is installed) shall not be listed in
itself (its hash can be retrieved by the sumsha256sums field of info file)

The file consists of a dictionary expressed in YAML where each installed file
is a key whose associated value is the hash of the file.

### example:

usr/bin/a-prog: a43d3be2987fab2cc5177f279dced0b3a3cf1773cd943c18a5d35981f147ecb7
usr/lib/liba: 17822ba7e3e60f55c475320fa745e5c3da4bc06ca8f608767d543358a20a0f7f
usr/share/doc/pkg-a/changelog: 67f682b05c81fed6af15ad2c4d8db0cb8341f4129fcfc7fe4b4c8084809dc8a8
MMPACK/symbols: 925312a655a8943ff4fabcf394ff2f76ba10aafc730a51e4a17c0ce4aa791605
MMPACK/post-install: 06410d79e7ee0b1ec2558ef8a282e163fd711a84d412ae7b2229fbff252c6579

## symbols file

YAML file contained in MMPACK folder in the binary package description of
symbols exported by ABI of a shared lib of in the package or other object
from language like python. Each entry is named after the soname of one
shared library and contains the follow entries:

 * depends: dependency name to add in the depending package. The minimal version
   specification will depends on the symbols used in the depending package.
 * symbols: list of symbols names exposed by the ABI. The value associated
   with a symbol correspond to the first version of the library that has
   exposed the symbol.

### example:

libmmlib.so.0:
    depends: libmmlib0
    symbols:
        mm_fstat: 1.0.0
        mm_fsync: 1.0.0
        mm_ftruncate: 1.0.0
        mm_get_lasterror_desc: 0.0.1
        mm_get_lasterror_extid: 1.0.0
        mm_get_lasterror_location: 0.0.1
        mm_get_lasterror_module: 0.0.1
        mm_get_lasterror_number: 0.0.1


## Package specification

### New format

Each package specification on one string containing the package name and
optionally in parenthesis the version constraints. It must respect the
following format:

.. code-block::

   <PKGNAME>[ (<OP> <VERSION>)]

where `OP` can be '=', '>=' or '<'


### Old format

Each package specification consists of one YAML entry whose key is the name of
the package and the value consisting in list of 2 versions: the minimal version
and the maximal. If 'any' is specified as minimal or maximal version, no
constraints as respectively minimal or maximal version. If the minimal version
is equal to the maximal veersion (and not specified as 'any'), the version of
the package must be equal to the specified version.
