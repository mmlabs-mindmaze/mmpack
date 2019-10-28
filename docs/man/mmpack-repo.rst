===========
mmpack-repo
===========

---------------
repo management
---------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>
:Date: 2019-10-29
:Manual section: 1

SYNOPSIS
========

``mmpack repo`` -h|--help
``mmpack repo`` add <name> <url>
``mmpack repo`` [list]
``mmpack repo`` remove <name>
``mmpack repo`` rename <old-name> <new-name>

DESCRIPTION
===========

**mmpack-repo** is a tool to manage the repositories of a prefix. It mimics
the *git remote* command. A call to **mmpack-update** is required for the
changes to take effect.

**mmpack repo add** adds a repository named **<name>** pointing to the
repository at **<url>**. The comand will fail to add a repository with a name
that already exists.

**mmpack repo list** shows a list of existing repositories.

**mmpack repo remove** removes the repository named **<name>**.

**mmpack repo rename** changes the name given to a repository from
**<old-name>** to **<new-name>**.

OPTIONS
=======
``-h|--help``
  Show help and exit

SEE ALSO
========
``mmpack``\(1),
``mmpack-mkprefix``\(1),
``mmpack-update``\(1),
