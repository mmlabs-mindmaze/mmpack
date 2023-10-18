package sources_strap structure
===============================

It is possible to separate the mmpack packaging from the actual source of the
project it package. In such a case, when generating the source tarball, the
upstream sources will be fetched and merged with mpack packaging.

name and location
-----------------

It should be called **sources-strap** and be placed within a folder named
**mmpack** at the root of the repository being packaged.  Following this naming
convention will allow mmpack to automatically find and fetch upstream source
automatically.

Format
------

The format follows the `yaml`_ format.

.. _yaml: https://yaml.org/

Fields
''''''

All fields are optional. However some might become mandatory in presence of
others.

 :method:
   The method how to fetch the upstream sources. It should be 'tar' or 'git'.

 :url:
   (mandatory if method set) The url used to recover the upstream sources of
   the project. With 'git' method, this url is the same as the argument for
   ``git clone``.

 :depends:
   list of mmpack dependencies needed to create the source package.

 :branch:
   (for git only) The commit hash, tag or branch that must be extracted
   by git clone

 :sha256:
   (for tar only) The SHA256 hash of the tarball that must be downloaded

 :noextract:
   (for tar only) Just copy the downloaded file without extracting the archive.

 :patches:
   list of patch files to applied on upstream sources after fetching them

 :guess-build-depends:
   Contains a dictionnary representing the configuration of the mechanism
   guessing the build-depends to add in package specs.

The guess-build-depends section
```````````````````````````````
It is possible to avoid to explicitly specify the build dependencies in the
package specs and to guess them from project sources. In such a case the
build-depends section of mmpack/specs is added when the mmpack source package
is created, the guessed dependencies being specified in addition to possible
build dependencies set already in package specs. The guess mechanism performed
per build system (not all are supported).

Several fields control the guess mechanism:

 :from:
   string or list of string of all build system that must be inspected. Can be
   python or all. If unset, all build system section listed are implied

 <build-system>
   Only python is currently supported.

The <build-system> section
..........................
  :ignore:
    list of guessed used dependencies that must not generate a mmpack build
    dependency.

  :remap:
    dictionary of used dependencies associated to the mmpack package name to
    use in the build-depends list. Use it to fix a bad guessed package name.


Token expansion
---------------

If `@MMPACK_VERSION@` or `@MMPACK_NAME@` tokens are found in the file, they are
expanded to respectively the version and the name of the source package
specified in **mmpack/specs**.


Examples
--------

.. code-block:: yaml

   method: git
   url: https://github.com/libusb/libusb.git
   branch: v1.0.23

.. code-block:: yaml

   method: tar
   url: https://mirrors.edge.kernel.org/pub/software/scm/git/git-2.24.0.tar.xz
   sha256: 9f71d61973626d8b28c4cdf8e2484b4bf13870ed643fed982d68b2cfd754371b

.. code-block:: yaml

   guess-build-depends:
     from: python

.. code-block:: yaml

   guess-build-depends:
     python:
       ignore: [Setuptools]
       remap:
         PyYaml: python3-yaml
