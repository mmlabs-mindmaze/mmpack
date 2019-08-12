===============
mmpack-provides
===============

-------------------------------------
search for a package from its content
-------------------------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-08-12
:Manual section: 1

SYNOPSIS
========

``mmpack provides`` [-h|--help]
``mmpack provides`` [-u|--update] *pattern*

DESCRIPTION
===========
**mmpack-provides** uses the repository file/package database to help knowing
which package provides given file or command.

The ``pattern`` argument can be any given string and is interpreted as
part of a file full path. Any path containing ``pattern`` will be shown.

The **mmpack-provides** subtool maintain a small database cache with the
relation between a file and its package. This cache is updated explicitly
using the update flag.

OPTIONS
=======
``-h|--help``
  Show help and exit

``-u|--update``
  Update local database of file/package correspondence

FILES
=====
``mmpack-file-db.yaml``
  File containing the relation explicitly a file and its package.

SEE ALSO
========
``mmpack``\(1),
``mmpack-install``\(1),
``mmpack-show``\(1),
``mmpack-search``\(1),
