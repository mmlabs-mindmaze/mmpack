# package structure

## file format

tar.xz file with extension .mpk containing:

 * files to install
 * MMPACK/info: YAML description of binary package
 * MMPACK/sha256sums: YAML dictionary of SHA256 hash of installed files
 * MMPACK/symbols: YAML description of symbols exported by ABI of a shared
   lib of in the package (if applicable)
 * MMPACK/pyobjects: YAML description of python objects (function or class)
   exported by the package (if applicable)
 * MMPACK/post-install: script to execute (if any) after package install
 * MMPACK/pre-uninstall: script to execute (if any) before package removal
 * MMPACK/post-uninstall: script to execute (if any) after package removal

Regarding post-install, pre-uninstall and post-uninstall files, currently,
only POSIX shell script are allowed. If there is a _real_ need in future
for other type of interpreter, there can be allowed if we add a predepends
field in info file.


## info file

YAML file located in the MMPACK folder in the binary package that describes the
package and its relationship with the other packages. It contains one entry
named after the package name with the following fields:

 * version: version of the package
 * source: name of the source package
 * sysdepends: list of package specification indicating the
   system dependencies of this package. This field is optional.
 * depends: list of package specification indicating the
   dependencies of this package. This field is optional.
 * conflicts: list of package specification that must be
   removed before the package being installed. This field is optional.
 * provides: metapackage that this package provides. This field is optional
 * description: string describing what the package does/provides
 * arch: string of the architecture/OS that this package is built for
 * sumsha256sums: SHA256 hash of list of installed files (MMPACK/sha256sums)
 * srcsha256: SHA256 hash of source package used to build this package
 * licenses: list of licenses used in the project

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

Each package specification consists of one YAML entry whose key is the name of
the package and the value is a relation ship (=, >=, <) and a package version
number. If the specification is not restricted by a particular version number or
range, the value any must be set.
