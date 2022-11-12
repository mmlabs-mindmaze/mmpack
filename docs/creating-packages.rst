.. _creating-mmpack-packages:

Creating mmpack packages
########################

.. include:: specs/package-specfile.rst

.. include:: specs/package-sources-strap.rst

.. include:: specs/build-scripts.rst

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

[1] https://github.com/mmlabs-mindmaze/mmpack-hello-world



You can test is as follows:

.. code-block:: sh

   # build mmpack-hello-world directly from git, using the top of the master
   # branch.
   mmpack-build pkg-create --tag master https://github.com/mmlabs-mindmaze/mmpack-hello-world

   # You can also clone and do the same from the project's folder:
   git clone "https://github.com/mmlabs-mindmaze/mmpack-hello-world"
   cd mmpack-hello-world
   mmpack-build pkg-create
   # This should end with telling you: "generated package: mmpack-hello-world"
   # the full name of your package should be in the line just before.

   # assuming you did not change your config, install from the created package
   # file within your home folder.
   # (on debian/x86, the * wildcard would expand into amd64-debian)
   mmpack install ~/.local/share/mmpack/packages/mmpack-hello-world_1.0.0_*.mpk

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
   PKG_FOLDER="$HOME/.local/share/mmpack-packages/"
   REPO="$HOME/mmpack-repo"
   ARCH=amd64-debian # amd64-windows for windows

   for manifest in $PKG_FOLDER/*.mmpack-manifest
   do
      mmpack-modifyrepo --path=$REPO --arch=$ARCH add $manifest
   done

   # add the repository to your list of repositories by adding file://$REPO
   # to your mmpack config "repositories" list
   # It could look like this
   cat > ~/.config/mmpack-config.yaml <<EOF
   repositories:
     - home-repo:
        url: file:///home/gandalf/mmpack-repo
        enabled: 1
   EOF


Alternatively you may create or update a repository by watching changes in a folder and
add the mmpack packages that fall in this folder:

.. code-block:: sh

   mmpack-modifyrepo --path=$REPO --arch=$ARCH watch $PKG_FOLDER


Managing repositories
#####################


.. include:: specs/repo.rst

