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
It is split into two parts: the **general** part, and a **custom** part for
package specifics.

The **general** part is named **general** and contain data concerning all the
the project itself, all the packages that can be build from it, and the
necessary information to build them.

The **custom** part is optional, and made of any number of entries, one for
each package it wants to customize.

.. _yaml: https://yaml.org/

The general section
```````````````````

Mandatory fields
''''''''''''''''

 :name:
   The source package name.
   This name is also used as root to generate the binary package names
   (unless they all are customized)

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
   A list of packages required to build the project.
   This is a key split into several sub-keys.
   Used alone, it can only contain *mmpack* packages.
   It can then be customized by platform

    - build-depends-debian-mmpack
        list of *mmpack* packages that are needed only when building on debian
    - build-depends-debian-system
        list of *debian* packages that are needed to build

 :ignore:
   list of files to be ignored by any packages.
   Any entry follows the `PCRE`_
   format.

.. _PCRE: https://www.pcre.org/current/doc/html/pcre2.html

The custom sections
```````````````````

The custom sections are used to change the default values of created packages,
or to create a custom specific package.
If a custom entry does not match a default mmpack target, then it will be
considered a custom packages.

List of fields to specify a package:

 :depends:
    A package **runtime mmpack** dependency
 :sysdepends-platform:
    Where **platform** is to be replace by the platform name
    (e.g. sysdepends-debian) is a platform **runtime** dependency
 :description:
      Any text describing the package
 :files:
     List of built files to include into the package.
     Follows the PCRE format.
     e.g. ``.*\.so`` to include all the files with ``so`` extension

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


Minimal specfile example
------------------------

.. code-block:: yaml

   general:
       name: mmpack-hello-world
       version: 1.0.0
       maintainer: Gandalf <gandalf@the.grey>
       url: ssh://intranet.mindmaze.ch:29418/mmlabs/mmpack-hello-world
       description: |
         mmpack hello world

