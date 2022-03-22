===================
mmpack-build-create
===================

-----------------------------------------
build mmpack package against repositories
-----------------------------------------

:Author: Nicolas Bourdaud <nicolas.bourdaud@gmail.com>
:Date: 2020-08-11
:Manual section: 1

SYNOPSIS
========

**mmpack-build create** [*option* ...] [*repo_url*]

DESCRIPTION
===========
**create** is the **mmpack-build** subcommand which builds the packages using the specified. It
produces the mmpack source packages exactly like what **mksource** subcommand
does and additionally, it builds the mmpack binary packages.

OPTIONS
=======
**create** supports all options that **mksource** subcommand supports plus
the following:

--skip-build-tests
  If passed, do not run the default test target after the build.
  Ie. make check or make test depending on the build system.

--repo-url=URL, -r URL
  Add the repository *URL* to the prefix created for the build. This option can
  be supplied multiple times to add several repositories.

--help, -h
  Show help and exit

EXAMPLE
=======
.. code-block:: sh

  # create mmpack package from local sources during development on a branch named *feature*
  mmpack-build create --repo-url=http://repo.url.com/subrepo --git --tag=feature /path/to/sources

  # create packages from source tarball with 2 repositories
  mmpack-build pkg-create --repo-url=http://repo1.url.com/subrepo1 --repo-url=http://repo2.url.com/subrepo2 --tar https://my.proj.com/project_1.2.3.tar.gz


SEE ALSO
========

**mmpack-build**\(1)
**mmpack-build_create**\(1)
**mmpack-build_mksource**\(1)
