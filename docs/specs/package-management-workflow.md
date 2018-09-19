# Package management workflow

## Preparation

For each package request to install
1. Package dependencies are computed
1. Missing dependencies are added to the list of action (as install) BEFORE
   the action of installing the depending package

For each package request to be removed
1. Depending installed packages are computed
1. Each depending package is added to list of action (as removal) BEFORE
   its dependent package removal

For the sake of dependencies solving, an upgrade of package version old to
version new is considered as a installation of new followed by removal of
old


## Install workflow

1. Files are unpacked
1. package is added in the list of installed package
1. post-install script is executed if any (argv: install)


## Upgrade workflow

A package is upgraded from version "old" to "new"

1. old-pre-uninstall is executed if any (argv: upgrade <new>)
1. list of installed files of old is gathered from sha256sums (old-list)
1. files of new are unpacked (overwriting old file if present). Remove from
   old-list any path that is unpacked by new.
1. new replace old in the list of installed package
1. files in old-list are removed
1. old-post-uninstall is executed if any (argv: upgrade <new>)
1. new-post-install script is executed if any (argv: upgrade <old>)


## Removal workflow

1. pre-uninstall script is executed if any (argv: remove)
1. list of installed files of old is gathered from sha256sums (old-list)
1. file in old-list are removed
1. package is removed from the list of installed package
1. post-uninstall script is executed if any (argv: remove)
