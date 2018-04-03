# package install

It's probably simpler to do this by group so as to handle circular dependencies.
If the group has no external dependency, we can safely disable dependency
checks during the group-install.

## 0. set-up
 * download package list
 * set up tmp work dir (if needed)
 * check if already installed

## 1. dependencies
 * install deps as a group
    * init the group as only containing the package
    * start with the initial package dependency list
 * foreach deps
    * if deps is part of the package list, ignore
    * test against *mmpack* package manager
      -> add to staged install if not
    * test against *system* package manager
      -> abort if not
    * add found dep to the install group
    * ... until no pkg dep is added
 * deps can be installed in any order
 * all deps found, goto 2.

THINK:
 * package update ?
 * there should be a few obvious solutions to a few obvious pitfalls.
   => list them here

## 2. install
 * check file list for conflict (abort if any)
 * install (below is debian order)
   - unpack
   - copy
   - set-up
   - post-install

THINK:
 * mmpack db ? which part of the pkg metadata should be installed

## 3. check ?

THINK:
 * orphaned packages
 * anything else comes to mind ?

## 3. cleanup
 * tmp work dir if needed
