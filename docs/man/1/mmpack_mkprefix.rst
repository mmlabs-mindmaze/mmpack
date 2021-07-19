===============
mmpack_mkprefix
===============

---------------------
create prefix command
---------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

**mmpack mkprefix** -h|--help

**mmpack mkprefix** [-f|--force] [-u|--upgrade] [--url= *repo_url*] [--name= *name*] [*prefix-path*]

DESCRIPTION
===========
**mmpack mkprefix** allows you to create a new prefix in folder specified by
*prefix-path* which will pull packages from *repo_url* whose URL is
optionally set by **--url** and whose short name is given optionally by
**--name**.

If **--upgrade** option is provided, the existing prefix settings will be kept.
Only the files will be upgraded.

If *prefix-path* is omitted, the argument of --prefix of mmpack and the
environment variable MMPACK_PREFIX are considered for determine the prefix
path.

If not present, the URL is inherited by the global user configuration of
mmpack. By default the command will prevent to create a prefix in a folder
that has been already setup.

OPTIONS
=======
--upgrade, -u
  Upgrade the prefix files.

--force, -f
  Force setting up prefix folder even if it was already setup.

--url=repo_url
  Specify *repo_url* as the URL of package repository

--name=name
  Specify *name* as the short name of the repository

--help, -h
  Show help and exit


SEE ALSO
========
**mmpack**\(1),
**mmpack_install**\(1),
**mmpack_remove**\(1),
**mmpack_run**\(1),
**mmpack_search**\(1),
**mmpack_update**\(1),
