Introduction
############

This is the documentation for the mmpack project.

mmpack is a cross-platform package manager.

It is designed to work without any need for root access, and to allow multiple
coexisting project versions within project prefixes (virtualenv-like sandboxes).

What mmpack does
================

The **mmpack** project is a set of tools to create and deploy packages in a
controlled fashion. To that end, it provides two main set of functions:
**mmpack**, and **mmpack-build**.

**mmpack** is a stub for all the tools required to fetch, install, remove ...
mmpack packages.

**mmpack-build** is a stub for all the tools required to create mmpack packages.

All mmpack and mmpack-build subcommands have their own man page which you can
find here: `List of mmpack subcommands`_

Details about more advanced features or concepts are detailed here:
`features`_

The goal
--------

 * Allow simpler package creation
 * Allow package installation without privileged access
 * Help when multiple unstable code versions must coexist
 * Help for demo packaging and deployment
 * Help with cross-platform development

List of mmpack subcommands
''''''''''''''''''''''''''

Below are html versions of all the man pages of the ``mmpack`` and
``mmpack-build`` subcommands.

All this information is available as man page, which means you can easily get
this information at any time.
e.g. type ``man mmpack install`` for info about the install subcommand of mmpack.

.. toctree::
   :titlesonly:
   :maxdepth: 1
   :glob:

   man/*/*

Features
--------

mmpack
''''''

 * Retrieve, install, remove ...
     All package management operations. See `List of mmpack subcommands`_

 * Dependency resolution
     When installing or removing packages, mmpack looks for all the impacted
     packages and takes the necessary actions:

     - when installing a package, mmpack looks for its dependencies, and submit
       the needed ones for install
     - when removing a package, mmpack looks for its reverse dependencies
       (packages that depend on the package you are removing), and remove them
       as well. This prevents leaving broken software installed.

 * mmpack prefixes
     mmpack prefixes are special directories in which you manage mmpack
     packages. The packages installed will have the usual folder tree (binaries
     in the bin folder, libraries in the lib folder ...)

     mmpack prefixes are similar in many ways to python's virtualenv.

     When within a mmpack prefix, the environment is momentarily enriched so
     that all the intended binaries, libraries, plugins, configuration files ...
     are used.

     The mmpack prefix in use can be specified with mmpack's ``--prefix`` option
     or with the ``MMPACK_PREFIX`` environment variable.

     When working within a prefix, mmpack is still aware of the system
     but will ignore any other mmpack prefixes you may have set up.

     You can choose to "work within a mmpack prefix" by running a single command
     with the ``mmpack run <command>``. You can also create a shell session
     within the prefix (e.g. ``mmpack run bash``) in which case all the commands
     called from the shell will be aware of the mmpack prefix environment when
     you call them.


mmpack-build
''''''''''''

 * Create packages
     When building packages from a project, mmpack will gather all the files
     that should be installed (staged for install) and use them to create
     several packages.

     By default, those packages are:

       - a main binary package
       - one package for each dynamic library
       - a devel package
       - a debug package
       - a documentation package

     More information about those packages can be found in the section
     about :ref:`creating-mmpack-packages` of this documentation.

 * Automatic dependency detection
     When creating a mmpack package, mmpack will go through all the files staged
     for install and scan for their dependencies. For each dependency found, it
     will determine the minimum required version and the package this
     dependency belongs to.

     To find this dependency package, mmpack initially looks for mmpack packages
     within the current prefix, and then in the host system.
     If no package is found, the package creation aborts and mmpack discards all
     installed files: if it were to create such a package, it would have
     dangling dependencies, and would not predictably run once installed.


What mmpack does not
====================

 * mmpack does not package system packages such as drivers: those will always
   require root privileges that mmpack cannot and must not have.

 * mmpack does not help with designing your project code or tool-chain. mmpack
   is merely layer above those.

 * mmpack does not isolate applications from each other, or the system; nor
   bring any **security** consideration to the table. mmpack is not a
   OS security tool.

 * mmpack does not provide cross-linux distribution portability by default.
   On the contrary, it is designed to work in concert with the host
   distribution.

 * mmpack does not provide any special guaranty about the packages it
   manipulates. It only manipulate them by working upon their metadata.

Comparison with other package managers
======================================

It works like any distribution package manager you may know: debian's dpkg/apt,
or centos/redhat rpm/yum/dnf.
However, mmpack is designed to lay upon the system package manager.
While a system package manager requires root access, mmpack does not require
any special permission to run and the packages are installed locally.

Another comparison point could be with language-specific package managers, such
as pip, or what go and rust have set in place.
Although its primary focus is on compiled languages, mmpack is not bound to any
kind of language.


Main commands
=============

The mmpack project mainly contains two sets of commands: **mmpack** and
**mmpack-build**. **mmpack** is the home for all commands about package
installation, removal, and other package manipulations. **mmpack-build**
is the home for all commands about *creating* mmpack packages.

The list of all commands provided by both programs can be found using
the ``--help`` options.

Note that **mmpack-build** requires **mmpack** installed to run correctly.
