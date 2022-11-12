Installing mmpack
=================

Supported platforms
-------------------

mmpack currently supports debian-based linux distributions, and windows >= 7.

It has only been tested on **Debian stable**, and **windows 10**.

Dependencies
------------

The **mmpack** install binary depends on the mmlib, the libyaml, the
libarchive, and the libcurl.

Installation instruction for the mmlib:
https://github.com/mmlabs-mindmaze/mmlib


The **mmpack-build** tool is written in **python3** and depends on the
following python3 modules: argcomplete, astroid, elftools, pefile, urllib3,
yaml.

The **mmpack-modifyrepo** tool is written in **python3** and depends on the
following python3 modules: watchdog, yaml.

See the developer's guide for a full breakdown of runtime, build-time, and
other dependencies.

Compiling from source
---------------------

mmpack uses the `Meson build system`_.

.. _`Meson build system`: https://mesonbuild.com/index.html

Below are the usual targets:

.. code-block:: sh

   mkdir build && cd build
   meson [options]
   ninja

   # run the tests
   ninja test

   # install (may require root permission, depending which prefix has been
   # configure when calling meson)
   ninja install

