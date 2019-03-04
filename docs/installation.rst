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
https://intranet.mindmaze.ch/mmlabs/gerrit/admin/repos/mmlib


The **mmpack-build** tool are written in **python3** and depends on the
following python3 modules: elftools, pefile, yaml.

See the developer's guide for a full breakdown of runtime, build-time, and
other dependencies.

Compiling from source
---------------------

mmpack uses **autotools**.

Below are the usual targets:

.. code-block:: sh

   ./autogen.sh
   mkdir build && cd build
   ../configure --help  # list the possible options
   ../configure [options]
   make

   # run the tests
   make check

   # build the doc (if configured with --enable-sphinxdoc)
   make html

   # install (may require root permission,
   # configure using --prefix so as to not to)
   make install

