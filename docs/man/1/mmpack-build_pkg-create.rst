=======================
mmpack-build-pkg-create
=======================

--------------------
build mmpack package
--------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.com>
:Date: 2020-08-11
:Manual section: 1

SYNOPSIS
========

**mmpack-build pkg-create** [*option* ...] [*path_or_url*]

DESCRIPTION
===========
**pkg-create** is the **mmpack-build** subcommand which builds the packages. It
produces the mmpack source packages exactly like what **mksource** subcommand
does and additionally, it builds the mmpack binary packages.

OPTIONS
=======
**pkg-create** supports all options that **mksource** subcommand supports plus
the following:

--skip-build-tests
  If passed, do not run the default test target after the build.
  Ie. make check or make test depending on the build system.

--prefix=path, -p path
  Use *path* as install prefix if needed.
  (DEPRECATED, use **mmpack-build --prefix** option instead)

--assumes-yes, -y
  Assume yes as answer to all prompts and run non-interactively.

--build-deps
  If a prefix has been passed as options, the mmpack packages will be installed
  within. If called within a mmpack prefix and no prefix option has been
  explicit, use it to install packages. If no prefix is explicit, and if
  **mmpack-build** is called outside any prefix, the default user global prefix
  will be used.
  (DEPRECATED, use **mmpack-build --install-deps** option instead)

  Whichever prefix is used, it needs to be correctly configured for this command
  to work.

--help, -h
  Show help and exit

EXAMPLE
=======
.. code-block:: sh

  # create mmpack package from local sources during development on a branch named *feature*
  mmpack-build pkg-create --tag=feature /path/to/sources

  # same but also skipping the tests
  mmpack-build pkg-create --tag=feature --skip-build-tests /path/to/sources

  # create arbitrary project on tag v1.2.3 from git server
  mmpack-build pkg-create --tag=v1.2.3 https://github.com/user/project

  # create packages from source tarball
  mmpack-build pkg-create --tar https://my.proj.com/project_1.2.3.tar.gz

  # create arbitrary project on tag v1.2.3 from git server installing build dependencies from http://repo.url.com/subrepo
  mmpack-build --repo-url=http://repo.url.com/subrepo pkg-create --tag=v1.2.3 https://github.com/user/project

  # create package from the current HEAD of local git project
  cd path/to/project
  mmpack-build

  # create package from the current project (file modified will be used)
  cd path/to/project
  mmpack-build pkg-create --path


SEE ALSO
========

**mmpack-build**\(1)
**mmpack-build_mksource**\(1)
