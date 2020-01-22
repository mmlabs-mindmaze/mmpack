======
mmpack
======

---------------
package manager
---------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2018-10-25
:Manual section: 1

SYNOPSIS
========

``mmpack`` --version

``mmpack`` -h|--help

``mmpack`` [-p|--prefix=*path*] *mmpack-command* [*mmpack-command options*]

DESCRIPTION
===========
**mmpack** is a cross-platform package manager.

It is designed to work without any need for root access, and to allow multiple
coexisting project versions within project prefixes (akin to python's
virtualenv sandboxes)

**mmpack** is the entry point for many package management commands (update,
install, remove ...).

Each **mmpack** command has its own man page and help option.

OPTIONS
=======

``--version``
  Display **mmpack** version

``-h|--help``
  Show help and exit

``-p|--prefix=*prefix-name*``
  if *prefix-name* is a path, use it as install prefix.
  Otherwise, use $XDG_DATA_HOME/mmpack-prefix/*prefix-name* as install prefix.
  Can also be given through ``MMPACK_PREFIX`` environment variable

mmpack-command
  Run given **mmpack** command.  Possible commands are:
  install, mkprefix, remove, run, search, update

ENVIRONMENT
===========
This section describes the environment variables that affect how
**mmpack** operates.


``MMPACK_PREFIX``
  Specify a prefix name.
  If it is a path, use it as install prefix.
  Otherwise, use $XDG_DATA_HOME/mmpack-prefix/$MMPACK_PREFIX as install prefix.
  This can also be given using the ``-p|--prefix`` flag.

EXAMPLE
=======

| # define a prefix to use throughout the whole session
| export MMPACK_PREFIX=/custom/prefix/folder
|
| # create a working prefix
| mmpack mkprefix --url=http://repository/url $MMPACK_PREFIX
|
| # update package list from given repository
| mmpack update
|
| # search for package name
| mmpack search name
|
| # install <package-name>
| mmpack install package-name

SEE ALSO
========
``mmpack-build``\(1),
``mmpack-install``\(1),
``mmpack-mkprefix``\(1),
``mmpack-remove``\(1),
``mmpack-run``\(1),
``mmpack-search``\(1),
``mmpack-show``\(1),
``mmpack-source``\(1),
``mmpack-update``\(1),
``mmpack-upgrade``\(1),
