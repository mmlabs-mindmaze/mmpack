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

**mmpack-build guess** update-provides *mmpack_pkg_build_dir*

DESCRIPTION
===========
**mmpack-build guess** will attempt to guess mmpack spec files. The actual
operation performed depends on the first positional argument. This argument is
assumed to be **create-specs**.

SUBCOMMANDS
===========
The subcommand can be one the values:

  create-specs
    Attempt to guess a mmpack specfile from the current tree and write it in
    **mmpack/specs** of the current directory. It expects to be called from the
    root of the git repository.

  update-provides
    Update provide specs (.provides files in mmpack folder) with the result of
    the build of the mmpack source package specified by *mmpack_pkg_build_dir*.

SEE ALSO
========
**mmpack-build**\(1),
