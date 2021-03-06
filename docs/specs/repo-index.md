# Repository index structure

## Index of binary packages

The index of binary packages is the file exported by the repository to share
the list of binary package available in it. It consists of one YAML file called
binary-index whose each entry describes a binary package. Each entry has the
same fields as the info file in the binary package structure, but in addition,
the following fields must be present:

 * filename: path on the repository server where the package can be
   downloaded from
 * sha256: SHA256 message digest of binary package
 * size: size in Bytes of the package to download


## Index of source packages

The index of source packages has the same role as the index of binary packages
but for the source packages provided by the repository. It is a file called
source-index. Each source package is described thanks to a set of keys and
values:

 * sha256: SHA256 of the source package.
 * name: name of the source package
 * filename: path on the repository server where the source package can be
   downloaded from
 * size: size in Bytes of the package to download
 * version: version of the source package

In the file each description of package is separated by an empty line.

## file/package database

This file is a database updated each time one of the packages of the repository
changes. It consists of a single, flat, YAML file called mmpack-file-db.yaml
whose entries are files and values the package providing them.
