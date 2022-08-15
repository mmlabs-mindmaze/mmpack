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
``````

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

 :patches:
   list of patch files to applied on upstream sources after fetching them


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
