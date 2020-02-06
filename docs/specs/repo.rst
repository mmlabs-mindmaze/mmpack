Server to upload
================

The file server.py (which calls functions from Repo.py) handles uploads of
packages in a specific repository (known by the server).
The server listens to a directory: when a file '.mmpack-manifest' is loaded in
this directory, the server tries to upload the packages listed in this file.

To upload the files, multiple actions are performed:
1) First the server moves the packages as well as the manifest file in a working
  directory.
2) Then the server checks that the manifest file is coherent with the packages
  provided by the user. In other words, it checks that the packages possess the
  characteristics indicated in the manifest file.
3) After the server starts to handle the upload: it lists the packages
  (registered in the manifest) that have to be moved in the repository in case
  the upload succeeds. It also lists the packages that have to be removed from
  the repository in case of success of the upload. Roughly speaking, the
  packages that have to be removed from the repository are packages such that
  new versions are being uploaded. While doing this, the server registers a
  binary index and a source index that respectively registers all the binary
  packages (mpk files) and all the source packages (tar.xz files) that should
  be present in the repository after the upload (in case it succeeds).
4) Then the server writes actual binary-index and source-index files containing
  the information collected during the previous step. These two files are
  written in the working directory.
5) If the previous step is a success then the upload can be achieved: the server
  just moves the binary-index and source-index, as well as all the packages
  listed in step 3 into the repository.

Note that until step 4 included, all the actions are performed in the working
directory. This permits to abort the execution easily in case a problem occurs.
The step 5 corresponds to the actual upload, and no problem may occur during
this step since all the actions have already been performed during the other
steps.
