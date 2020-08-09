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

**mmpack-build pkg-create** [--skip-build-tests] [--git | --src | --mmpack-src | --path] [-t|--tag *tag*] [-y|--yes] [--build-deps] [path_or_url]

**mmpack-build pkg-create** -h|--help

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

--skip-build-tests
  If passed, do not run the default test target after the build.
  Ie. make check or make test depending on the build system.

--git
  Interpret *path_or_url* argument as git url or path to a local git repository.

--src
  Interpret *path_or_url* argument as source package tarball.

--mmpack-src
  Interpret *path_or_url* argument as mmpack source package tarball. **mmpack-build** will
  preserve this tarball as it is and pass it directly to binary packages build
  stages.

--path
  Interpret *path_or_url* argument as folder containing mmpack packaging specification.

--tag=tag, -t tag
  The project's tag to use.
  If a git repository (local or remote url) is being built, this will specify
  the commit to build. This can actually be any kind of object recognized by
  the ``git --tag`` option:

    * a git tag
    * a commit sha1
    * a branch name

  For the other type of build, *tag* will be the string which will be used to
  identify a package build in the cache folder.

--prefix=path, -p path
  Use *path* as install prefix if needed.

--assumes-yes, -y
  Assume yes as answer to all prompts and run non-interactively.

--build-deps
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

--help, -h
  Show help and exit

EXAMPLE
=======
.. code-block:: sh

  # create mmpack package from local sources during development on a branch named *feature*
  mmpack-build pkg-create --git --tag=feature /path/to/sources

  # same but also skipping the tests
  mmpack-build pkg-create --git --tag=feature --skip-build-tests /path/to/sources

  # create arbitrary project on tag v1.2.3 from git server
  mmpack-build pkg-create --git --tag=v1.2.3 https://github.com/user/project

  # create packages from source tarball
  mmpack-build pkg-create --tar https://my.proj.com/project_1.2.3.tar.gz

  # create package from the current HEAD of local git project
  cd path/to/project
  mmpack-build

  # create package from the current project (file modified will be used)
  cd path/to/project
  mmpack-build pkg-create --path


SEE ALSO
========

**mmpack-build**\(1)
