===============
mmpack-remove
===============

--------------
remove package
--------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

``mmpack install`` -h|--help

``mmpack remove`` [-y|--assume-yes] *pkg1* [*pkg2* [...]]

DESCRIPTION
===========
**mmpack-remove** removes given packages and the packages depending upon them
from the current prefix.

First *mmpack** adds all given packages and the packages depending upon them
to the current transaction. If any additional package is required, it will
it will ask for user confirmation before proceeding (default answer is negative).
Otherwise it will proceed to the removal.


Note:  **mmpack remove** and **mmpack uninstall** are aliases.

OPTIONS
=======
``-h|--help``
  Show help and exit

``-y|--assume-yes``
  Assume yes as answer to all prompts and run non-interactively.

SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-install``\(1),
``mmpack-run``\(1),
``mmpack-search``\(1),
``mmpack-update``\(1),
