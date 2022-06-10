mmpack FAQ
##########


Why mmpack at all ?
   The **mmpack** project is a package manager that works
   - cross-platform
   - without root access
   - allows several concurrent conflicted versions to coexist

   As far as we know, this is the only tool that allows all of this.


Who is mmpack designed for ?
   mmpack is designed with several sets of people in mind.

   First for collaborating R&D teams, to help when multiple unstable code
   versions must coexist, to help with cross-platform development, and to help
   with setting up demo packaging and deployment.

   Second for technico-marketing teams to set up said demo easily, without
   special setup especially permission issues

   Finally for anyone developing cross-platform and user-space projects.


Are all mmpack packages available on all supported platforms ?
   The goal is **yes**. However, nothing is preventing a package to only
   be available for a single platform ...
   If a package is not available for your platform, and that platform is
   supported by mmpack, you should contact the package maintainer.


How does mmpack handle dependencies ?
   Both **mmpack** and **mmpack-build** should be handling dependencies for
   you. **mmpack** by installing dependencies on install, and removing
   back-dependencies on removal. **mmpack-build** by detecting which package
   you used to build, find its minimum required version, and adding it as
   dependency in the created package.


How do I ...
============

... retrieve the source code of a package ?
   ``mmpack source <package-name>``

... get information about the mmpack packages I'm installing
   ``mmpack show <package name>``

... get back the same mmpack prefix I used yesterday ?
   The first time - only the first time - you need to create the mmpack prefix
   using ``mmpack mkprefix``. It will create the folders you need.
   Then you tell mmpack to work with the prefix, either by using the
   ``--prefix`` option, or ``MMPACK_PREFIX`` environment variable.
   Any time later (after leaving the shell, rebooting ...) you can use this
   same prefix with only the second step.
   The recommended way is to use the ``--prefix`` option in scripts, and to
   set the ``MMPACK_PREFIX`` environment variable when working in you shell.

... find out what went wrong ?
   - for **mmpack** commands, see ``var/log/mmpack.log`` within the mmpack
     prefix.
   - for **mmpack-build** see mmpack log file (``mmpack.log``) in the build
     folder of the project.

... ask for help, or report an issue ?
   If your issue is about a specific package, contact the package maintainer
   directly.

   If your issue is about mmpack itself, contact the
   *nicolas.bourdaud@gmail.com*.

... know what dependency mmpack will see and use ?
   When **mmpack** installs package X depending on package Y, if package Y is
   not already installed within the working prefix, then mmpack will install
   it, regardless of whether Y is already installed in another prefix, provided
   by another system package, or even installed manually by yourself.

   When **mmpack-build** looks for dependencies during the package creation, it
   looks for mmpack packages in the current mmpack prefix first, then in the
   installed system packages if one meets the requirements needs. If you
   manually installed the requirements (not using mmpack nor a system manager)
   then it will NOT be considered.

... get more information about a specific mmpack command ?
   *All* mmpack and mmpack-build commands and subcommands have a ``--help``
   option that should give you the information you are looking for.
   e.g. ``mmpack --help``, ``mmpack install --help`` ...
   If any such help is missing, incomplete, or unintelligible, please contact
   us.

... know which mmpack prefix is active in my interactive shell
   Inspect MMPACK_ACTIVE_PREFIX environment variable. You may modify your
   shell prompt to reflect whether a prefix is currently active and which
   one. A bash script (prompt.bash) provided in the installation of mmpack
   implements this and can be sourced in your bash initialization file.
