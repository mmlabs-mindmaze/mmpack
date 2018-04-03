# package creation

## 0. set-up
 * set-up working tree
 * check package syntax
 * check git tree is clean
 * get host environment
   (arch, system pkg manager ...)

THINK:
 * recover previous package (version, ...)
 * generate changelog (optional)

## 1. package sources
 * git archive ...
 * copy packaged sources

THINK:
 * does the source archive contain the packaging files ?
   (updated changelog ?)
   (updated symbol file ?)

## 2. build package
 * detect/read build system
 * configure
 * build
 * check

THINK:
 * other predefined steps ?
   (doc ?)

## 3. local-install
 * install
 * strip
 * ventilate
 * compress

THINK:
 * anything missing ?

## 4. packaging
 * get+gen symbol list
 * gen deps
   1. deps type ? (lib, python ...)
   2. try to resolve using *mmpack* package db
   3. try to resole using *system* package manager
   4. assert(not-reached-here)
 * hash, sign, ...
 * copy result to out folder

## 5. clean
 * optional
