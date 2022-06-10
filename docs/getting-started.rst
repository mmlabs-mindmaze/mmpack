Getting Started
###############

mmpack
======

Once mmpack is installed, you can start setting up a prefix for it, and
set up repositories to pull from, and install some packages.

.. code-block:: sh

   # It is somewhat easier to carry the prefix used across all commands
   # by setting it in an environment variable.
   # Choose a working directory for the prefix
   export MMPACK_PREFIX=/custom/prefix/folder

   # Find a mmpack repository filling you needs.
   # Then, create a working mmpack prefix
   mmpack mkprefix --url=http://repository/url

   # update your local list of available packages
   mmpack update

   # install a package
   mmpack install <package-name>


See ``mmpack --help``, or the mmpack man pages for infos about all the mmpack
commands. Most of them include an example to illustrate what they do.


About mmpack repository configuration
-------------------------------------

mmpack packages repositories that you always need can be setup in your global
mmpack configuration.
This configuration file is located in the ``$XDG_CONFIG_HOME`` folder (defaults
to $HOME/.config). The mmpack configuration file is called
**mmpack-config.yaml**.

Should the mmpack configuration file have a *repositories* section filled, it
can be used implicitly.

mmpack-build
============

mmpack-build is used to create mmpack packages.
In order to package a project, you need two things:

 - a mmpack specfile describing you projects (see example below)
 - all the dependencies needed to run your project should be packaged
   (by mmpack or by the hosting distribution - e.g. Debian)

Emphasis on the fact that you MUST package all the dependencies of your project
**before** being able to package the project itself, otherwise you cannot
guarantee that the package you create can be deployed safely on other
environments.

Here is an example specfile:

.. code-block:: yaml

   name: hello-world-project
   version: 1.0.0
   maintainer: Gandalf <gandalf@the.grey>
   url: https://github.com/the-grey/hello-world-project
   description: >
     This is a fake project named hello world.


mmpack-build will detect your build system, build the package, scan through the
generated artifacts for external dependencies, and associate the necessary
packages at their minimum required version for you.

Here are some examples on how to call mmpack-build:

.. code-block:: sh

   # called from the package you want to package
   mmpack-build pkg-create

   # create mmpack package from local sources during development
   # on a branch named *feature*
   mmpack-build pkg-create --tag=feature /path/to/sources

   # same but also skipping the tests
   mmpack-build pkg-create --tag=feature --skip-build-tests /path/to/sources

   # create arbitrary project on tag v1.2.3 from git server
   mmpack-build pkg-create --tag=v1.2.3 https://github.com/the-grey/hello-world-project.git


