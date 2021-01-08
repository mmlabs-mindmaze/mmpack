===========
mmpack_repo
===========

---------------
repo management
---------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>
:Date: 2019-10-29
:Manual section: 1

SYNOPSIS
========

**mmpack** [options] **repo** [**list**]

**mmpack** [options] **repo add** <name> <url>

**mmpack** [options] **repo remove** <name>

**mmpack** [options] **repo rename** <old-name> <new-name>

**mmpack** [options] **repo enable** <name>

**mmpack** [options] **repo disable** <name>

**mmpack repo** -h|--help

DESCRIPTION
===========

**mmpack repo** is a tool to manage the repositories of a prefix. It mimics
the *git remote* command. A call to **mmpack update** is required for the
changes to take effect. It supports the following subccommands:

   add
      adds a repository named *name* pointing to the repository at *url*. The
      comand will fail to add a repository with a name that already exists.

   list
      shows a list of existing repositories. This is the default command.

   remove
      removes the repository named *name*.

   rename
      changes the name given to a repository from *old-name* to *new-name*.

   enable
      makes the repository named *name* enabled, i.e., the packages provided by
      the repository *name* are available after the next call to mmpack update.

   disable
      makes the repository named *name* disabled, i.e., the packages provided
      by the repository *name* are not available anymore.

OPTIONS
=======

-h|--help
   Show help and exit

SEE ALSO
========
**mmpack**\(1),
**mmpack_mkprefix**\(1),
**mmpack_update**\(1),
