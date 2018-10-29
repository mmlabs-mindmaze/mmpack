# pacman packaging files

This contains the required files to create the pacman packages for mmpack and
its dependencies.

curl and yaml are already packaged and part of msys2; this only adds mmlib.

## howto

*This asserts that the sources will be located in $HOME/sources/<package>*
ie. "~/sources/mmlib", "~/sources/mmpack" ...

To create a pacman package, move to the folder containing the PKGBUILD file,
the makepkg command will copy the sources, build local-install, and create
an archive.

``` bash
# move to pacman's mmlib folder
cd pacman/mmlib

# create mingw64 pacman's mmlib package
MINGW_INSTALLS=mingw64 makepkg-mingw --syncdeps --log  --force

# peek into the generated package
tar tvf mingw-w64-x86_64-libmmlib0-0.3.4-1-x86_64.pkg.tar.xz

# install the result locally
pacman -U mingw-w64-x86_64-libmmlib0-0.3.4-1-x86_64.pkg.tar.xz

# check that it is installed
pacman -Qqe | grep mmlib

# remove/uninstall the package
pacman -R mingw-w64-x86_64-libmmlib0
``` 
