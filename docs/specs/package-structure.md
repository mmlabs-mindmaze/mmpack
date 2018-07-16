# package structure

## file format

tar.xz file with extension .mpk containing:

 * files to install
 * MMPACK/info: YAML description of binary package
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
