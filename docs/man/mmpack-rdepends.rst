===============
mmpack-rdepends
===============

-----------------------------------------
show the reverse dependencies of packages
-----------------------------------------

:Author: Marjorie Bournat <marjorie.bournat@mindmaze.ch>
:Date: 2019-11-04
:Manual section: 1

SYNOPSIS
========

``mmpack rdepends`` -h|--help

``mmpack rdepends`` [-r|--recursive] [--repo=*repo-name*] *package*[=*version*]

DESCRIPTION
===========
**mmpack-rdepends** show the reverse dependencies of a given package.

OPTIONS
=======
``-h|--help``
  Show help and exit

``-r|--recursive``
  Ask for a recursive search of the reverse dependencies

``--repo= *repo-name*``
  Name of the repository where to search for the reverse dependency

**package**
  Name of the package for which the reverse dependencies are searched

SEE ALSO
========
``mmpack``\(1),
