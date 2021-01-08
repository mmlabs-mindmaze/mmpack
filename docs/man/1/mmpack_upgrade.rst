==============
mmpack_upgrade
==============

---------------
upgrade command
---------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-02-19
:Manual section: 1

SYNOPSIS
========

**mmpack upgrade** [-y|--assume-yes] [*pkg* [...]]

**mmpack upgrade** [-h|--help]

DESCRIPTION
===========

**mmpack upgrade** upgrades given packages to their latest possible version.

If not package is given, **mmpack upgrade** installs all available upgrades
of the packages currently installed in the current prefix.

New packages may be installed if required to satisfy new dependencies.

OPTIONS
=======
--help, -h
  Show help and exit

--assume-yes, -y
  Assume yes as answer to all prompts and run non-interactively.


SEE ALSO
========
**mmpack**\(1),
**mmpack_install**\(1),
**mmpack_list**\(1),
**mmpack_mkprefix**\(1),
**mmpack_remove**\(1),
**mmpack_search**\(1),
**mmpack_update**\(1),
