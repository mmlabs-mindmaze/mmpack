===============
mmpack-mkprefix
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

``mmpack mkprefix`` -h|--help

``mmpack mkprefix`` [-f|--force] --url= *repository* --name= *name* *prefix-path*

DESCRIPTION
===========
**mmpack-prefix** allows you to create a new prefix in folder specified by
*prefix-path* which will pull packages from the repository whose URL is
optionally set by ``--url`` and whose short name is given optionally by
``--name``.

If not present, the URL is inherited by the global user configuration of
mmpack. By default the command will prevent to create a prefix in a folder
that has been already setup.

OPTIONS
=======
``-h|--help``
  Show help and exit

``-f|--force``
  Force setting up prefix folder even if it was already setup.

``--url= *repository*``
  Specify repository as the address of package repository

``--name= *name*``
  Specify name as the short name of the repository

**prefix-path**
  Folder within which to create the prefix


SEE ALSO
========
``mmpack``\(1),
``mmpack-install``\(1),
``mmpack-remove``\(1),
``mmpack-run``\(1),
``mmpack-search``\(1),
``mmpack-update``\(1),
