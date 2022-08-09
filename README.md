# **mmpack** - a user package manager

**mmpack** is a cross-platform, multi-versioning user package manager (as
opposed to system package manager). It leverages the container namespace
virtualisation technique to isolate package installations which allows
separate, non-conflicting ecosystems.

In the mmpack jargon, these ecosystems are called _prefixes_ and ressemble the
equivalent Python virtualenvs or Ruby gemsets.  However, mmpack packages are
not targeting any specific language, instead, they aim to bundle any meaningful
set of files (including binaries, data and documentation).

mmpack is designed to work without any need for root access, and to allow
multiple coexisting project versions within project prefixes.

mmpack leverages [mmlib](https://github.com/mmlabs-mindmaze/mmlib) and it is
hence fully cross-platform. Currently supported systems are GNU/Linux and
Windows.

For questions, requests, issues, pull-requests, etc., drop an email:
`nicolas.bourdaud@gmail.com`

## Main features

* Easy package creation
* User space installation
* Coexistence of different versions of the same software
* Deployment support (quick process to setup cross-platform packages)
* Built-in package distribution utility

## Installation

**mmpack** depends on [*mmlib*](https://github.com/mmlabs-mindmaze/mmlib),
*libcurl*, *libarchive* and *libyaml* and the python3 modules: *argcomplete*,
*astroid*, *elftools*, *pefile*, *urllib3*, *watchdog*, *yaml*.

After downloading the code, change to the source folder and type the following
to install mmpack:

``` bash
$> meson build && cd build
$> meson install
```

In GNU/Linux system also the _setcap_ command is required (with root
permission);

``` bash
#> ninja setcap
```

That allows mmpack to use the kernel namespace isolation for the prefixes and
it is the last time root permission is ever needed while using mmpack.

#### Debian systems

Debian users can take advantaged from a packaged version of mmpack. In debian
systems just add the file _/etc/apt/sources.list.d/mmpack.list_, containing:

```
deb http://opensource.mindmaze.com/debrepos unstable main
deb-src http://opensource.mindmaze.com/debrepos unstable main
```

The gpg key must also be added in the apt gpg folder:

``` bash
curl https://opensource.mindmaze.com/debrepos/mindmaze-opensource.gpg | sudo tee /etc/apt/trusted.gpg.d/mindmaze-opensource.gpg > /dev/null
```

Then it is a matter of (as root or with sudo):

``` bash
apt update
apt install mmpack
```

#### Windows systems (MSYS2)

Windows users with the MSYS2 system can edit _/etc/pacman.conf_ and add the
following lines:

```
[mmlabs]
Server = https://opensource.mindmaze.com/mingw
```

Then run:

``` bash
pacman -Sy
pacman -S mmpack
```

## Usage

mmpack comes with two CLI commands;

 * mmpack: manage prefixes and install software;
 * mmpack-build: create packages.

#### On prefixes

A prefix is identified by a path which can be specified in different ways;
through the command line argument --prefix and through the environment variable
$MMPACK_PREFIX.

Each prefix can be associated with one or several upstream repositories for
installing software right away; to create a prefix run:

``` bash
mmpack mkprefix --url=<http://repository/url> $MMPACK_PREFIX
```

(See the section below about installing software through repositories)

Process run outside the prefixes do not have access to their content and
processes run inside one prefix have the privileges of the running user plus
access to their own prefix (but not the others).

This means that, for a given software A with dependencies B,C, it is possible
to have:

 * a system wide installation at version x, with B,C
 * another installation at version y in prefix _j_
 * another installation at version z in prefix _k_, with patched B

Moreover, installation in a prefix does not require root privileges (provided
it is inside a user accessible folder).

#### Installing upstream software

mmpack sports some of the most common commands among package managers:

``` bash
# update package list from the repository url(s) of prefix <prefix_name>
mmpack --prefix=<prefix_name> update

# search for available package in the prefix <prefix_name>
mmpack --prefix=<prefix_name> search <package-name>

# install <package-name> in the prefix <prefix_name>
mmpack --prefix=<prefix_name> install <package-name>
```

Other useful commands include _list_ and _upgrade_.
All the commands support the _--help_ flag for discoverability.

All the commands provide the _--prefix_ flag, however it is also possible to
use the environment variable $MMPACK_PREFIX; the following commands have hence
the same effect as the above ones:

``` bash
export $MMPACK_PREFIX=<prefix_path>

# update package list from the repository url(s) of prefix <prefix_path>
mmpack update

# search for available package in the prefix <prefix_path>
mmpack search <package-name>

# install <package-name> in the prefix <prefix_path>
mmpack install <package-name>
```

#### Run mmpack software

To run mmack software, just invoke the `<command>` with:

``` bash
mmpack run <command>
```

Referring to the example for software A:

``` bash
A #  runs the system-wide installed version
mmpack --prefix=j run A #  runs A from prefix j
mmpack --prefix=k run A #  runs A from prefix k, which loads patched B
```

If no command is provided, the default $SHELL is launched.  Inside this shell
all the programs installed in the current prefix are added to the $PATH.
(Hence, typing _mmpack run_ is to some extent similar to mounting an extension
to the root filesystem).

Again, with respect to the above example,

``` bash
mmpack --prefix=j run #  enters prefix j environment
A #  runs A from prefix j
```

For the ease of use, the possibility to specify prefixes with an environment
variable has been introduced; so the following two invocations have the same
effect;

``` bash
mmpack --prefix=j run A #  runs A from prefix j

export MMPACK_PREFIX=j
mmpack run A #  runs A from prefix j
```

#### Package creation

Creating mmpack packages has been designed to be easy-peasy.  mmpack-build
supports the most widespread build-systems: Makefile, Autotools, Meson.

mmpack-build just needs the so-called _mmpack/spec_-file; a simple example is
given by

``` yaml
name: mmpack-hello-world
version: 1.0.0
maintainer: Gandalf <gandalf@the.grey>
url: ssh://<your git url>
description: mmpack hello world
```

The build-system to use can be specified; if it is not, mmpack will guess it
for you.

Refer to the [documentation](https://opensource.mindmaze.com/projects/mmpack/)
for in-depth details about spec files.  Provided sources and the mmpack/spec
file, building the package is as easy as typing:

``` bash
mmpack-build pkg-create
```

## Documentation

Documentation can be found online, https://opensource.mindmaze.com/projects/mmpack/ or
generated from sources.
