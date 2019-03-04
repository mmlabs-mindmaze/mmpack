Developer’s Guide
#################

This section is about contributing to mmpack.

We use gerrit (https://intranet.mindmaze.ch/mmlabs/gerrit/) as code review
tool. If you do not know gerrit, there is a standard debian package called
**git-review** which helps you interacting with gerrit (basically
just call "git review" once you are ready to send a commit for review).

The master branch is ... the *master* branch. We fork to create releases, and
currently only maintain the last *major* release while working on a new one.
This means that the master branch receives the current development; all commits
go through unit testing in continuous integration (CI).

CI tests for:

 - python codespell compliance
 - python pylint compliance
 - unit tests

There are additional helper commands such as "make spelling" to help you spot
typos.

Dependency breakdown
====================

Runtime dependencies
--------------------

mmpack
``````
 * libyaml
 * libcurl
 * libarchive

mmpack-build
````````````

 * python3
 * python3 `elftools` module
 * python3 `pefile` module
 * python3 `yaml` module

When creating a package, mmpack-build builds the project from source.
This means that you also must be able to build it.

mmpack-build has a set of pre-defined build plugins for the usual build
systems. Each plugin requires the corresponding tool-chain to be installed
to work.

Available plugins are:

 * cmake
 * meson
 * autotools
 * make (pure makefile)
 * python (python setuptools)

A project using a custom tool-chain not listed above can make use of the
``mmpack/build`` specific specfile to build, in which case, you should
inspect that file to know what the required dependencies are.

Build dependencies
------------------

You will need the usual (essential) build tools like ``make``, ``gcc``,
``perl`` ...

The mmpack project uses the **autotools** build system, which is therefore
needed to build.

mmpack
``````

 * mmlib (https://intranet.mindmaze.ch/mmlabs/gerrit/admin/repos/mmlib)
 * libyaml development files
 * libcurl development files
 * libarchive development files

mmpack-build
````````````

None.

Tests
`````
All runtime dependencies, plus:

 * check

Documentation
`````````````

To generate the user documentation :

 * sphinx
 * sphinx-rtd-theme

Additional requirement to generate the internal development documentation:

 * linuxdoc (https://github.com/GabrielGanne/linuxdoc)

Test tools
==========

The first test tool is the "make check" target, which is the one that will be
called by the CI

There are two main development scripts in the **devtools** folder:

 - **test-shell.sh**, spawns a shell with a local-installation of mmpack
   and a clean mmpack prefix.
 - **pkg-create-tests.sh**, creates several mmpack packages.
   Call with the SMOKE variable set to only build and test mmpack-hello-world.

Memcheck suppression file
=========================

mmpack dependencies (mostly through libcurl) have some memory leaks. Those are
intentional and not bugs. They are identified and listed in the
**devtools/mmpack.supp** file. Call as follows:

.. code-block:: sh

   echo $PWD
   # ~/mmpack/build
   valgrind --suppressions=../devtools/mmpack.supp -- mmpack update
