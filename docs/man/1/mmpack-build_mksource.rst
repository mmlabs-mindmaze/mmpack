=====================
mmpack-build-mksource
=====================

---------------------------
build mmpack source package
---------------------------

:Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.com>
:Date: 2020-08-11
:Manual section: 1

SYNOPSIS
========

**mmpack-build mksource** [*option* ...] [*path_or_url*]

DESCRIPTION
===========
**mksource** is the **mmpack-build** subcommand which prepares the source
package of a project. It builds the source packages from the location specified
in *path_or_url* argument.

If no argument is provided, it looks through the current directory or one of
its parent for a mmpack folder, and use the containing folder as root
directory.

If the root directory does not contains mmpack packaging specification
(**mmpack/specs**) and instead a file called **mmpack.projects**, the root
directory is considered to be a mmpack multi projects. In that case, this file
is assumed to contains the list (element in each line) of path to an individual
mmpack packaging specification (path relative to folder containing
**mmpack.projects**). **mksource** subcommand will then generate the source
package of each listed subprojects (unless **--multiproject-only-modified** is
provided to options).

For the each mmpack source package produced, the command will print on a line
of standard output the name of the project, the version packaged and the path
to the mmpack source package produced.

OPTIONS
=======

--git
  Interpret *path_or_url* argument as git url or path to a local git repository.

--src
  Interpret *path_or_url* argument as source package tarball.

--mmpack-src
  Interpret *path_or_url* argument as mmpack source package tarball. **mmpack-build** will
  preserve this tarball as it is and pass it directly to binary packages build
  stages.

--path
  Interpret *path_or_url* argument as folder containing mmpack packaging specification.

--tag=tag, -t tag
  The project's tag to use.
  If a git repository (local or remote url) is being built, this will specify
  the commit to build. This can actually be any kind of object recognized by
  the ``git --tag`` option:

    * a git tag
    * a commit sha1
    * a branch name

  For the other type of build, *tag* will be the string which will be used to
  identify a package build in the cache folder.

--multiproject-only-modified[= *commit*]
  If specified, in case of mmpack multi projects, the command will generate the
  mmpack source package of only subfolders modified by the git commit being
  checked out. If optional value *commit* is provided, projects whose
  subprojects have been modified since *commit* will be build.

--update-version-from-vcs
  update version stored in the generated source package as update since tag
  named after the version in the specs.

--stop-on-error
  When building multiple projects, stop after the first build failure
  encountered. By default, all projects built are attempted, but the global
  return code will indicate failure if any of the builds fails.

--help, -h
  Show help and exit


SEE ALSO
========

**mmpack-build**\(1)
**mmpack-build_pkg-create**\(1)

