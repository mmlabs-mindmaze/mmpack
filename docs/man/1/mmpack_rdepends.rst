===============
mmpack_rdepends
===============

-----------------------------------------
show the reverse dependencies of packages
-----------------------------------------

:Author: Marjorie Bournat <marjorie.bournat@mindmaze.ch>
:Date: 2019-11-04
:Manual section: 1

SYNOPSIS
========

**mmpack rdepends** [-r|--recursive] [--sumsha] [--repo=*repo-name*] *package*[=*version*]

**mmpack rdepends** -h|--help

DESCRIPTION
===========
**mmpack rdepends** show the reverse dependencies of a package named *package*
for which the reverse dependencies are searched. In the case where the option
**--sumsha** is set, *package* corresponds to the sumsha of the package.


OPTIONS
=======
--recursive, -r
  Ask for a recursive search of the reverse dependencies

--repo=repo-name
  Name of the repository where to search for the reverse dependency

--sumsha
  Ask to search for the reverse dependencies of the package referenced thanks to
  its sumsha.

--help, -h
  Show help and exit


SEE ALSO
========
**mmpack**\(1),
