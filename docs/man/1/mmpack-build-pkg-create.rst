=======================
mmpack-build-pkg-create
=======================

--------------------
build mmpack package
--------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2020-01-14
:Manual section: 1

SYNOPSIS
========

``mmpack-build pkg-create`` -h|--help

``mmpack-build pkg-create`` [--skip-build-tests] [--git | --src | --mmpack-src | --path] [-t|--tag *tag*] [-y|--yes] [--build-deps] [path_or_url]

DESCRIPTION
===========
**mmpack-build-pkg-create** is the **mmpack-build** subcommand which builds the
packages. It builds the mmpack binary and source packages from the location
specified in *path_or_url* argument.

If no argument is provided, it looks through the current directory or one of
its parent for a mmpack folder, and use the containing folder as root
directory.

OPTIONS
=======

``-h|--help``
  Show help and exit

``--skip-build-tests``
  If passed, do not run the default test target after the build.
  Ie. make check or make test depending on the build system.

``--git``
  Interpret *path_or_url* argument as git url or path to a local git repository.

``--src``
  Interpret *path_or_url* argument as source package tarball.

``--mmpack-src``
  Interpret *path_or_url* argument as mmpack source package tarball. **mmpack-build** will
  preserve this tarball as it is and pass it directly to binary packages build
  stages.

``--path``
  Interpret *path_or_url* argument as folder containing mmpack packaging specification.

``-t|--tag= *tag*``
  The project's tag to use.
  If a git repository (local or remote url) is being built, this will specify
  the commit to build. This can actually be any kind of object recognized by
  the ``git --tag`` option:

    * a git tag
    * a commit sha1
    * a branch name

  For the other type of build, *tag* will be the string which will be used to
  identify a package build in the cache folder.

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
