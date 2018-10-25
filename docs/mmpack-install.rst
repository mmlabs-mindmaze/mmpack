===============
mmpack-install
===============

----------------
install packages
----------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Version: 1.0.0
:Manual section: 1

SYNOPSIS
========

``mmpack install`` *pkg1* [= *version1*] [*pkg2* [= *version2*] [...]]

DESCRIPTION
===========
**mmpack-install** installs given packages and their dependencies into the
current prefix.

If **mmpack** finds missing systems dependencies, then it will abort the
installation and request said packages.

If **mmpack** finds additional packages required for the transaction
(dependencies) it will ask for user confimation before proceeding (default
answer is negative). Otherwise it will proceed to the installation.


SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-remove``\(1),
``mmpack-runprefix``\(1),
``mmpack-search``\(1),
``mmpack-update``\(1),
