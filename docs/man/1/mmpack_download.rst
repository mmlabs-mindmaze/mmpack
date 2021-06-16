===============
mmpack_download
===============

-----------------
download packages
-----------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-08-20
:Manual section: 1

SYNOPSIS
========

**mmpack download** *pkg1* [= *version1*]

**mmpack download** -h|--help

DESCRIPTION
===========
**mmpack download** downloads given packages into the current mmpack prefix
cache folder. The package itself is not parsed by mmpack, and its dependencies
are not looked for.

The package file is put within the current folder, and may be installed either
by passing it directly to the **mmpack install** command, or by moving it to
the package cache of the mmpack prefix (**$MMPACK_PREFIX**) located in the path:
**<PREFIX>/var/cache/mmpack/pkgs** .

OPTIONS
=======
--help, -h
  Show help and exit


SEE ALSO
========
**mmpack**\(1),
**mmpack_mkprefix**\(1),
**mmpack_search**\(1),
**mmpack_source**\(1),
**mmpack_install**\(1),
