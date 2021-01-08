=============
mmpack_update
=============

--------------
update command
--------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

**mmpack update [-h|--help]**

DESCRIPTION
===========

**mmpack update** update local package list from repository list in config file.

OPTIONS
=======
--help, -h
  Show help and exit

FILES
=====
**/etc/mmpack-config.yaml**
  Config file containing the repositories URL.

**/var/lib/mmpack/binindex.yaml**
  Local cache for mmpack package metadata.

SEE ALSO
========
**mmpack**\(1),
**mmpack_install**\(1),
**mmpack_mkprefix**\(1),
**mmpack_remove**\(1),
**mmpack_run**\(1),
**mmpack_search**\(1),
**mmpack_upgrade**\(1),
