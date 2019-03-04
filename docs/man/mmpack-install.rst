===============
mmpack-install
===============

----------------
install packages
----------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

``mmpack install`` [-y|--assume-yes] *pkg1* [= *version1*] [*pkg2* [= *version2*] [...]]

``mmpack install`` -h|--help

DESCRIPTION
===========
**mmpack-install** installs given packages and their dependencies into the
current prefix.

If **mmpack** finds missing systems dependencies, then it will abort the
installation and request said packages.

If **mmpack** finds additional packages required for the transaction
(dependencies) it will ask for user confirmation before proceeding (default
answer is negative). Otherwise it will proceed to the installation.

OPTIONS
=======
-h|--help
  Show help and exit

-y|--assume-yes
  Assume yes as answer to all prompts and run non-interactively.

SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-remove``\(1),
``mmpack-run``\(1),
``mmpack-search``\(1),
``mmpack-source``\(1),
``mmpack-update``\(1),
``mmpack-upgrade``\(1),
