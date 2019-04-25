==========
mmpack-run
==========

---------------------
run command in prefix
---------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

``mmpack run`` [-h|--help] [*command*]

DESCRIPTION
===========
**mmpack-run** runs given command within current prefix.
If no command was given, it will open an interactive shell within the current
prefix instead.


OPTIONS
=======
**command**
  The command to run, as if to the shell prompt.

``-h|--help``
  Show help and exit

EXAMPLE
=======

| # create a bash shell within current prefix
| mmpack run bash


ENVIRONMENT VARIABLES
=====================

While a process is running in a prefix (ie children of the **mmpack-run**
command process), the following variables are set :

``MMPACK_PREFIX``
  Although this variable may have been set prior to **mmpack-run**, this is
  reset to the absolute path of the active prefix mounted by **mmpack-run**.
  It may however be reset again in the children of this command if, while a
  prefix is active, mmpack commands are intended to operate on a different
  prefix.

``MMPACK_ACTIVE_PREFIX``
  Defined to the absolute path of the active prefix. This variable is to be
  read only as an indicator of the currently mounted prefix.


SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-install``\(1),
``mmpack-remove``\(1),
``mmpack-search``\(1),
``mmpack-update``\(1),
