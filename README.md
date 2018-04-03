# RFC: package manager

I intend to fill it with comments, bits of code or snippets
until the time a clear picture emerge and this can be presented
for comments (before the real coding)

This is open for all and any comments all the time though.
Email me: gabriel.ganne@mindmaze.ch

# A few decisions:
* targets *debian* and *windows*
* python3
* all files will be in **yaml** if in any format
* mmpack is the entry point for **all** mmpack-reladed commands (git-like)
* python **MUST** comply with pycodestyle, and *SHOULD* comply with pylint
* use pre-defined package build tree (rpmbuild style)
* use debian way of handling symbols and dependancies
* do not handle any system packages (but have dependancies upon them).
  Should one be needed and not installed, abort and request its installation
* enforce tag and version format: tag **MUST** be of format "vx.y.z[-whatever]"
  version will be extracted with format x.y.z from the tag.
  tag string **CANNOT** contain the '/' (slash) character.
  See: [Semantic Versioning](https://semver.org/spec/v2.0.0.html)

# Ideas
* build server for cross compilation
* virtualenv-like feature
* create new package from previous one

# SGP
[Original SGP link](https://www.google.co://intranet.mindmaze.ch/confluence/display/SD/Package+Manager+--+SGP+Level+1)

* Business Case	
    * Package management:
        * Automate packaging for:
            * Linux packages (Debian 8, Debian 9)
            * Windows 10
            * Android ?
        * Support packages dependencies downloads.
        * Provides fetchable repository mechanism per released version and tags (though git).
            * installation with dependencies. (How to deal with system deps?)
            * upgrades.
            * Support release vs debug packages. (to which point?)
            * Support development packages vs binary packages.
            * Automated installation feasible for Jenkins, or a regular user. (UC: Automatic Reinstallation of a dev MMPRO)
            * Flag certain packages as 3rd parties.
    * Package View:
        * Permits to browse and download packages in prettified pages. (For release validation and on need)
    * Cross-platform (more or less):
        * Works on Debian 8, Debian 9, Windows 10. Ubuntu ?
        * Compatible with work using Microsoft Visual Studio, Unity 3D, CLion (Cmake).
    * Multiprojects:
        * Co-installation of different projects packages set.
    * Easy to use
        * Compatible with Johnny?
        * Possible to install without root/admin access.
        * Fast setup time.
        * Package generation feasible by Jenkins, but (rather) easy by a regular dev.
        * Support available packages from several lists.
* Scope:
    * Automate packaged dependencies management.
    * Same (more or less) setup on both windows/linux.
    * Rather easy creation of package.
    * Automation of package creation.
* Out of scope:
    * System packages that requires root installation (siso packages).
    * Cross-compilation.
