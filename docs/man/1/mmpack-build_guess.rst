============
mmpack-guess
============

----------------------------
guess package specifications
----------------------------

:Author: Gabriel Ganne <gabriel.ganne@mindmaze.ch>
:Date: 2019-12-17
:Manual section: 1

SYNOPSIS
========

**mmpack-build guess** [create-specs]

DESCRIPTION
===========
**mmpack-build guess** will attempt to guess mmpack spec files. The actual
operation performed depends on the first positional argument. This argument is
assumed to be **create-specs**.

SUBCOMMANDS
===========
The subcommand can be one the values:

  create-specs
    Attempt to guess a mmpack specfile from the current tree and prints it to
    stdout. It expects to be called from the root of the git repository.

SEE ALSO
========
**mmpack-build**\(1),
