Usual mmpack commands
#####################

This is a basic example on how to create a new mmpack prefix in your profile
named **mmpack-prefix**, install the **eegview** package within it, and then to
run it.

.. code-block:: sh

   # mmpack repository to pull the packages from
   MMPACK_REPOSITORY=http://mm-v008/mmpack/stable/debian/

   # define a prefix to use throughout the whole session
   export MMPACK_PREFIX=$HOME/mmpack-prefix

   # create a working prefix
   mmpack mkprefix --url=$MMPACK_REPOSITORY $MMPACK_PREFIX

   # update package list from given repository
   mmpack update

   # search for eegview
   mmpack search eegview

   # install eegview
   # Note: eegview has dependencies (libeegdev0, libmcpanel0 ...)
   # They should be pulled as well, and mmpack will ask for your
   # consent before proceeding.
   mmpack install eegview

   # run the eegview you just installed
   mmpack run eegview

typically when you write a command mmpack as follow:
mmpack command name

the name on the commandline can represent:
    - a path to the package
    - the package name
    - the string "package_name=version"
    - the string "package_name=key:value", where "key:"
    could be equal to "hash:" or to "repo:"



Note on running binaries installed with mmpack
==============================================

When running a binary installed using mmpack (for example eegview) you NEED to
call them using the ``mmpack run`` command. This is for several reasons.
The main one is that you can have multiple instances of the same binary
available on your station - for example a system one, and a mmpack one.
Also, mmpack will run the *right* one, including the correct configuration
files associated to your binary, and use the correct libraries.

Ensuring that several installations of the same binary do not conflict is not
a trivial operation if you come to think about it; this should do the job for
you.
