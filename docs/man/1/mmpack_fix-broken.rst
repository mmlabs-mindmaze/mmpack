=================
mmpack_fix-broken
=================

-------------------
fix broken packages
-------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>,
         Nicolas Bourdaud <nicolas.bourdaud@mindmaze.ch>
:Date: 2019-02-18
:Manual section: 1

SYNOPSIS
========

**mmpack fix-broken** [*pkg* [...]]

**mmpack fix-broken** -h|--help

DESCRIPTION
===========
**mmpack fix-broken** resets an already-installed package to its original
state, meaning as if it was just installed.

if no argument is given, **mmpack fix-broken** will go through all the
installed packages to try and fix them, otherwise it will only process the
packages given in the command line.

An example use case can be, if the sha256sums file is corrupted or missing.
In this case, it is impossible to remove successfully a package (the file is
used to describe the broken package content). However, the package can at the
same time be installed, and even be working as expected.
In this described case, fix-broken will only repair the sha256sums file.

**mmpack fix-broken** only works on already-installed packages.


SEE ALSO
========
**mmpack**\(1),
**mmpack_install**\(1),
**mmpack_remove**\(1),
**mmpack_upgrade**\(1),
