Server to upload
================

The file server.py (which calls functions from Repo.py) handles uploads of 
packages in a specific repository (known by the server).
The server listens to a directory: when a file tar is loaded in this directory,
the server tries to upload the packages of the tar.

The file tar provided to the server MUST contained a manifest file containing
metadatas on binary and source packages, as well as these packages.

To upload the file tar multiple actions are performed:
- First the server checks that the manifest file in the tar is coherent with the
  packages provided by the user. In other words, it checks that there exist
  files in the tar that possess the characteristics indicated in the manifest
  file.
- Once the tar has been checked, the server starts the upload: it takes the 
  packages of the tar that are registered in the manifest and move them into the
  repository. While doing this it while update



This server is duplicated on 4 service systemctl in order to
