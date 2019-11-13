.. _creating-mmpack-packages:

Creating mmpack packages
########################

.. include:: specs/package-specfile.rst

.. include:: specs/package-sources-strap.rst

Generating binary packages
==========================

When building packages from a project, mmpack will create one or several
packages from the files staged for installation.
The packages declared in the mmpack specfile will receive the files according
to **files** fields specified there. The remaining files are assigned to either:

 - a main binary package
 - one package for each dynamic library
 - a devel package
 - a debug package
 - a documentation package


This process can be detailed as follows (for package "XXX"):

0. Gather all the staged files
1. Give the custom packages their requested files
2. Give files to their corresponding package based on their type.
   Create the packages as needed.

   - documentation files (man pages, sphinx outputs ...) go to ``XXX-doc``
   - debug symbols go to ``XXX-debug``
   - development files (headers, static libraries ...) go to ``XXX-devel``
   - dynamic libraries create their own packages
   - binaries, and ``.1`` man pages go to the main binary package ``XXX``

3. All the remaining files (if any) will go to a *default* package.
   The *default* package is the main binary package if there is one.
   If there is no binary package and **only one** library package,
   the default library is the library package.
   Finally, if there is no binary package, and there are multiple
   library packages, create a main binary package as host for those files.


Some filters are not explicit here because they  *should* be intuitive to
understand.
If you have an issue with they way this works, most of them are implemented in
``src/mmpack-build/file_utils.py`` should you feel up to it, otherwise contact
us and we'll try to help.


Minimal mmpack packaging example
================================

A minimal mmpack packaging example is available here [1].
It is an autotools-based hello-world project with simple commands.
It also is used as part of the mmpack continuous testing.
Feel free to fork it, and play around with it.

[1] https://intranet.mindmaze.ch/mmlabs/gerrit/admin/repos/mmlabs/mmpack-hello-world


You can test is as follows:

.. code-block:: sh

   # prepare to work on a tmp prefix
   MMPACK_REPOSITORY=http://mm-v008/mmpack/stable/debian/
   export MMPACK_PREFIX=/tmp/mmpack-hello-world-prefix

   # create a mmpack prefix
   mmpack mkprefix --url=$MMPACK_REPOSITORY $MMPACK_PREFIX

   # clean the mmpack-build workspace, temporary files, and previously
   # generated packages
   mmpack-build clean --wipe

   # build mmpack-hello-world directly from git, using the top of the master
   # branch.
   mmpack-build pkg-create --git-url ssh://intranet.mindmaze.ch:29418/mmlabs/mmpack-hello-world --tag master

   # You can also clone and do the same from the project's folder:
   git clone "ssh://intranet.mindmaze.ch:29418/mmlabs/mmpack-hello-world"
   cd mmpack-hello-world
   mmpack-build pkg-create
   # This should end with telling you: "generated package: mmpack-hello-world"
   # the full name of your package should be in the line just before.

   # assuming you did not change your config, install from the created package
   # file within your home folder.
   # (on debian/x86, the * wildcard would expand into amd64-debian)
   mmpack install ~/.local/share/mmpack-packages/mmpack-hello-world_1.0.0_*.mpk

   # you can test the package by running
   hello-world
   mmpack run hello-world

   head-libexec-world  # should fail
   mmpack run head-libexec-world  # this one should work


How to create a local repository using the package I created ?
==============================================================

.. code-block:: sh

   # assuming you did not change your config, the mmpack packages are created
   # in the following folder
   REPO="$HOME/.local/share/mmpack-packages/"

   mmpack-createrepo $REPO $REPO

   # add the repository to your list of repositories by adding file://$REPO
   # to your mmpack config "repositories" list
   # It could look like this
   cat ~/.config/mmpack-config.yaml
   repositories:
     - file:///home/gandalf/.local/share/mmpack-packages
