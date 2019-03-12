=======================
mmpack-build-pkg-create
=======================

--------------------
build mmpack package
--------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-10-25
:Manual section: 1

SYNOPSIS
========

``mmpack-build pkg-create`` -h|--help

``mmpack-build pkg-create`` [--skip-build-tests] [--git-url= *url* | --src= *tarball*] [-t|--tag *tag*] [-y|--yes] [--build-deps]

DESCRIPTION
===========
**mmpack-build-pkg-create** is the **mmpack-build** subcommand which builds the packages.

OPTIONS
=======

``-h|--help``
  Show help and exit

``--skip-build-tests``
  If passed, do not run the default test target after the build.
  Ie. make check or make test depending on the build system.

``--git-url= *url*``
  Can be either:

    * a git url of the repository to build
    * a path to a local git repository

``--src= *tarball*``
  path to a source package tarball

``-t|--tag= *tag*``
  The project's tag to use.
  If ``--git-url`` is specified, this can actually be any kind of object
  recognized by the ``git --tag`` option:

    * a git tag
    * a commit sha1
    * a branch name

  If ``--src``, this can be any string which will be used to identify a
  package build in the cache folder

``-p|--prefix= *path*``
  Use *path* as install prefix if needed.

``-y|--yes``
  Assume yes as answer to all prompts and run non-interactively.

``--build-deps``

  Check for build dependencies:

    * abort on missing system ones.
    * install mmpack missing ones.

  If a prefix has been passed as options, the mmpack packages will be installed
  within. If called within a mmpack prefix and no prefix option has been
  explicit, use it to install packages. If no prefix is explicit, and if
  **mmpack-build** is called outside any prefix, the default user global prefix
  will be used.

  Whichever prefix is used, it needs to be correctly configured for this command
  to work.


SEE ALSO
========

``mmpack-build``\(1)
