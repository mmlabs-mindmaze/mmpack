===========
mmpack-list
===========

-------------
list packages
-------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>
:Date: 2018-12-21
:Manual section: 1

SYNOPSIS
========

``mmpack list`` [-h|--help] [all|available|upgradable|installed|extras] [*pattern*]

DESCRIPTION
===========
**mmpack-list** lists information about the installed or available packages.

With no argument specified, defaults to **installed** which lists all installed
packages only.

**mmpack list all** lists both installed and available packages

**mmpack list available** lists all the packages that are provided through any
of the registered repositories.

**mmpack list upgradable** lists all installed packages that could be upgraded.
This means that a newer version of the package is available.

**mmpack list installed** lists all the packages that are installed in the
current prefix.

**mmpack list extras** lists all the packages that are installed but are
unavailable through any of the provided repositories. This usually means
such packages was installed manually from an archive.


Additionally, if a package, or a package pattern is given, the results will be
filtered by that pattern.

OPTIONS
=======
``-h|--help``
  Show help and exit

SEE ALSO
========
``mmpack``\(1),
``mmpack-search``\(1),
``mmpack-show``\(1),
``mmpack-update``\(1),
