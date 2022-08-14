package specfile structure
==========================

name and location
-----------------

The mmpack specfile should be called **specs** and be placed within a folder
named **mmpack** at the root of the repository being packaged.
Following this naming convention will allow mmpack to automatically find
and work with the package specfile.

Format
------

The specfile follow the `yaml`_ format.

.. _yaml: https://yaml.org/

Mandatory fields
''''''''''''''''

 :name:
   The source package name.
   This name is also used as root to generate the binary package names
   (unless they all are customized).
   For ghost packages, this field will also be used to search for binary
   package in the system package repository. All binary system package whose
   source is this name will be fetched to create ghost packages. This source
   name to search in the repo can be changed with syspkg-srcnames option.

 :version:
   The version of the packages to be generated.
   It also is used in the name generation.
   e.g. package name "**XXX**" version "**1.0.0**" will generate by default:
   (on arch amd64-debian)

    - source package: ``XXX_1.0.0_src.tar.gz``
    - binary package: ``XXX_1.0.0_amd64-debian.mpk``
    - devel package: ``XXX-devel_1.0.0_amd64-debian.mpk``
    - doc package: ``XXX-doc_1.0.0_amd64-debian.mpk``

 :maintainer:
   The email address of the maintainer (the person a user should contact if
   something goes wrong)

 :url:
   The url used to recover the upstream sources of the project.
   On git, this url is the same as the argument for ``git clone``

 :description:
   Any text describing the project.

Optional fields
'''''''''''''''

 :build-options:
   Any custom compilation flags that will be added to the build
   when creating the package.
   e.g. ``-D_WITH_CUSTOM_DEFINE=1``

 :build-system:
   Which build plugin to use. e.g. "cmake" to use the cmake build
   plugin (build-cmake file). Or the **custom** wildcard to tell
   mmpack to build using a provided build plugin ``mmpack/build``
   See build-plugin-specs.md on how to write such a thing.

 :build-depends:
   A list of packages required to build the project. It can only contain *mmpack* packages.

 :ghost:
   True if the packaging specs refers to a ghost of system packages.
   If not present, False is assumed.

 :syspkg-srcnames:
   Meaningful only if ghost is True. Optional key-value mapping allowing
   to determine to which source name the system binary packages must be
   associated when searched in the repository of system package. If the key
   matches the target distribution, the corresponding value will be used as
   source project name instead of name field in the mmpack spec. However name
   field will still be used to generate the name of the mmpack ghost package.
   The key is a PCRE regular expression used to match targetted host
   distribution of the generated package.

 :ignore:
   list of files to be ignored by any packages.
   Any entry follows the `PCRE`_
   format.

 :licenses:
   List of licences for the project. Each entry can be either a path to the
   project's license file, or a license name as present in mmpack's
   common-licenses folder.
   If left unset, will fallback to either LICENSE, or COPYING file from the
   source tree (case insensitive).
   Note: the "licenses" field is optional, but its value is mandatory for
   mmpack-build to work. If no license could be found, mmpack-build will
   abort.

 :copyright:
   Can be either a string, or a path to a copyright file.
   The copyright file has a free format without any special specifications.
   The copyright field and value is entirely optional (unlike the "licenses"
   field).

 :custom-pkgs:
   This contains a dictionary of packages must be created maybe in addition to
   automatic packages. If an automatic package should have been created whith
   the same name of one of the custom packages, the settings in the custom
   sections serves as overriding of the settings that should have been used for
   the automatic package.

.. _PCRE: https://www.pcre.org/current/doc/html/pcre2.html

The custom-pkgs sections
````````````````````````

The custom sections are used to change the default values of created packages,
or to create a custom specific package.
If a custom entry does not match a default mmpack target, then it will be
considered a custom packages.

List of fields to specify a package:

 :depends:
    A list of package **runtime mmpack** dependencies
 :sysdepends-platform:
    Where **platform** is to be replace by the platform name
    (e.g. sysdepends-debian) is a platform **runtime** dependency
 :description:
      Any text describing the package
 :files:
     List of built files to include into the package.
     Follows the PCRE format. The regular expression will match the whole path
     relatively to the prefix root.
     e.g. ``.*\.so`` to include all the files with ``so`` extension,
     ``bin/program(\.exe)?`` to include **program** executable.

Note on the default packages
````````````````````````````

For project XXX, the default packages usually are as follows:

 - A XXX binary package with the binaries themselves, the (1) section of the man
   pages, the locale files ...
 - A XXX-devel package containing the development files (this is mostly true for
   projects that are libraries)
 - A XXX-doc package that will contain any generated documentation.
 - If the package contains a library libyyy.so, then a package will be created
   for it.

Special values
''''''''''''''

For any string field (scalar value in YAML terminology) can be expanded with
the content of a file. To this end, the `file()` macro is available. Pass the
path of the target file in the argument to get its content loaded as the value
of field like:

.. code-block:: yaml

   version: file(../VERSION)

The path loaded is relative to the directory containing the loaded spec file.


Examples
--------

Minimal specfile example
''''''''''''''''''''''''

.. code-block:: yaml

   name: mmpack-hello-world
   version: 1.0.0
   maintainer: Gandalf <gandalf@the.grey>
   url: https://github.com/mmlabs-mindmaze/mmpack-hello-world
   description: |
     mmpack hello world


Ghost package specfile example
''''''''''''''''''''''''''''''

.. code-block:: yaml

   name: mmpack-hello-world
   version: 1.0.0
   maintainer: Gandalf <gandalf@the.grey>
   url: https://github.com/mmlabs-mindmaze/mmpack-hello-world
   description: |
     mmpack hello world
   ghost: true
   syspkg-srcnames:
       debian: mmpack-h-w # project source is known as mmpack-h-w in Debian
       fedora: mmpack-helloworld # project source is known as mmpack-helloworld in fedora


Old specfile format
-------------------

An outdated specfile format was used in early version of mmpack. It is the same
Yaml hierarchy, excepting that all settings were put in a `general` section and
the custom packages were defined (if any) at the same level of the general
section (consequently, no package named general could be created).
