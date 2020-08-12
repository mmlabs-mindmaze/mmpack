=================
mmpack-modifyrepo
=================

------------------------
modify mmpack repository
------------------------

:Author: Nicolas Bourdaud <nicolas.bourdaud@mindmaze.com>
:Date: 2020-07-07
:Manual section: 1

SYNOPSIS
========
**mmpack-modifyrepo** [*options*] **watch** *upload_dir*

**mmpack-modifyrepo** [*options*] **add** *manifest_file*

**mmpack-modifyrepo** [*options*] **batch**

**mmpack-modifyrepo** -h|--help


DESCRIPTION
===========
**mmpack-modifyrepo** is used to build both binary and source packages indices.
The repository to be modified/created is current directory but this can be
changed with the **--path** option. If the repository folder do not exist yet,
the folder along with its metadata files will be created. The architecture used
will be then the one specified by the **--arch** option.

Once the repository has been opened or created, the behavior of
**mmpack-modifyrepo** will depend on the specified subcommand.

watch subcommand
----------------
The folder *upload_dir* is watched for modification. Whenever a manifest file
is copied into it, it will be processed and the packages matching the
architecture of the repository will be added to the repository. The binary and
sources packages listed in the manifest are assumed to be copied in *upload_dir*
before the manifest is itself copied. Once a manifest file is successfully
processed the related mmpack source and binary package are eventually moved to
the repository folder and the source and binary indices files are updated
accordingly.

add subcommand
--------------
This processes the mmpack manifest specified by *manifest_file*. The source
package will be added to the repository along with the binary package matching
its architecture. The source and binary indices are updated and the command
returns afterward. The success of the operation is reported in the exit code of
the command.

batch subcommand
----------------
The standard input is read line by line. In each line a command can be issued
(empty line are ignored):

   ADD
      take the path to a manifest written on the rest of the line and stage the
      changes that would be carried to the repository if **mmpack-modifyrepo
      add** were called. However changes are not yet visible in the repository
      folder.

   BEGIN_CHANGES
      start staging changes that will be applied to repository

   COMMIT_CHANGES
      commit the changes that have been staged so far. The content of the
      repository folder will then show the changes. This command cannot fail.
      
   ROLLBACK_CHANGES
      cancel the stagged changes and revert internal state to the one after the
      last commit or the initial state when **mmpack-modifyrepo batch** was
      called. This command cannot fail.

   COMMIT
      combines COMMIT_CHANGES with BEGIN_CHANGES

   ROLLBACK
      combines ROLLBACK_CHANGES with BEGIN_CHANGES

For each line of command issued, when the operation is finished, the result is
reported on one line of standard output with **OK** in case of success, or
**FAIL** in case of failure. In such a case, the error message the reported on
the rest on the line.


OPTIONS
=======

--help, -h
   Show help and exit

--path=repo_path, -p repo_path
   Use *repo_path* to locate the repository to modify. If not specified, it is
   assumed to be the current directory.

--arch=arch_string, -a arch_string
   If created, the repository will provide the architecture specified by
   *arch_string*. If unspecified with a new repository, the command will fail.
   If the repository was already created, the option will be ignored.
