===============
mmpack-builddep
===============

-----------------
builddep packages
-----------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-01-08
:Manual section: 1

SYNOPSIS
========

**mmpack-build builddep** [-p|--prefix=*path*] [-y|--yes] [*path/to/specfile*]

**mmpack-build builddep** -h|--help

DESCRIPTION
===========
**mmpack-build builddep** downloads and install the build dependencies of given
packages into the current prefix.
It will also check for the presence of mentioned system packages.

**mmpack-build builddep** will succeed if all the system packages are already
installed and if all the mmpack packages are already present or successfully
added by the current command.
It will fail otherwise.


OPTIONS
=======

--help, -h
  Show help and exit

--prefix=path, -p path
  Use *path* as install prefix.
  (DEPRECATED, use **mmpack-build --prefix** option instead)

--assume-yes, -y
  Assume yes as answer to all prompts and run non-interactively.

ENVIRONMENT
===========

MMPACK_PREFIX
  If set, **mmpack-build** will parse the given prefix tree when looking for
  package dependencies. If unset, **mmpack-build** will only use the system
  tree.


SEE ALSO
========
**mmpack**\(1),
**mmpack_source**\(1),
**mmpack_install**\(1),
**mmpack_mkprefix**\(1),
**mmpack-build**\(1),
