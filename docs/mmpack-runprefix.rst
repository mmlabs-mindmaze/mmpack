================
mmpack-runprefix
================

---------------------
run command in prefix
---------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Version: 1.0.0
:Manual section: 1

SYNOPSIS
========

``mmpack runprefix`` *command*

DESCRIPTION
===========
**mmpack-runprefix** runs given command within current prefix.


OPTIONS
=======
**command**
  The command to run, as if to the shell prompt.

EXAMPLE
=======

| # create a bash shell within current prefix
| mmpack runprefix bash


SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-install``\(1),
``mmpack-remove``\(1),
``mmpack-search``\(1),
``mmpack-update``\(1),
