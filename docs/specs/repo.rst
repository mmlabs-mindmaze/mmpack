Server to upload
================

The file server.py (which calls functions from Repo.py) handles uploads of
packages in a specific repository (known by the server).
The server listens to a directory: when a file tar is loaded in this directory,
the server tries to upload the packages of the tar.

The file tar provided to the server MUST contained a manifest file containing
metadatas on binary and source packages, as well as these packages.

To upload the tared files, multiple actions are performed:
- First the server checks that the manifest file in the tar is coherent with the
  packages provided by the user. In other words, it checks that there exist
  files in the tar that possess the characteristics indicated in the manifest
  file.
- Once the tar has been checked, the server starts the upload: it takes the
  packages of the tar that are registered in the manifest and move them into the
  repository. While doing this, the server updates the binary index and the 
  source index that respectively register all the binary packages (mpk files)
  and all the source packages (tar.xz files) that are present in the repository.
- When the upload of the packages is achieved, the server remove the bonary and
  source packages that are not needed anymore. For instance a source package is
  not needed anymore if a new version of the source package has been uploaded by
  the user.

Actually this server is duplicated on 4 services systemctl in order to handle
the uploads on the 4 following repositories:
- stable debian repository located on 
  https://opensource.mindmaze.com/mmpack/stable/debian which contains stable
  versions of packages executable on debian.
- unstable debian repository located on 
  https://opensource.mindmaze.com/mmpack/unstable/debian which contains unstable
  versions of packages executable on debian.
- stable windows repository located on 
  https://opensource.mindmaze.com/mmpack/stable/windows which contains stable
  versions of packages executable on windows.
- unstable windows repository located on
  https://opensource.mindmaze.com/mmpack/unstable/windows which contains
  unstable versions of packages executable on windows.

There is no need to launch this 4 services, they are automatically launched
by systemctl when the machine, on which the server is hosted, is on.

If needed, to stop one of the services, one can write on a terminal:
.. code:: 
   sudo systemctl stop name_service

Where *name_service* is either debian_stable.service, debian_unstable.service,
windows_stable.service, or windows_unstable.service that handles respectively
the uploads of packages on the stable debian repository, the unstable debian
repository, the stable windows repository, and the unstable windows repository.

If needed, to stop one of the services, one can write on a terminal:
.. code::
   sudo systemclt start name_service
