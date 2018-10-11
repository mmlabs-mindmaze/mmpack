# **mmpack** package manager

**mmpack** is a cross-platform package manager.

It is designed to work without any need for root access, and to allow multiple
coexisting project versions within project prefixes (virtualenv-like sandboxes).

Please not that **mmpack** is now in *alpha*: there are no guarantees on any
of its interfaces.

If you have questions, requests, issues, pull-requests ...
Please email me: `gabriel.ganne@mindmaze.ch`

## goal

* Allow simpler package creation
* Help when multiple unstable code versions must coexist
* Help for demo packaging and deployment
* Help with cross-platform development

## usage

### package installation

#### *mmpack* command

```
Usage:
  mmpack [options] mkprefix <repo-url>
  mmpack [options] update
  mmpack [options] install <pkg1>[=<version1>] [<pkg2>[=<version2>] [...]]
  mmpack [options] remove <pkg1> [<pkg2> [<pkg3> [...]]]

Options:
  -p PATH, --prefix=PATH      Use PATH as install prefix.
  -h, --help                  print this message and exit
```

### pakage creation

#### *mmpack-build* command
```
Usage:
mmpack <command> [command-options]

Show full mmpack help
mmpack --help

List all mmpack commands:
mmpack --list-commands

More infos about a specific command:
mmpack <command> --help
```

#### *mmpack-build pkg-create* subcommand
```
usage: mmpack-build pkg-create [-h] [-s URL] [-t TAG] [-p PREFIX]
                               [--skip-build-tests]

Create a mmpack package

Usage:
mmpack pkg-create [--url <path or git url>] [--tag <tag>]
                  [--prefix <prefix>] [--skip-build-tests]

If no url was given, look through the tree for a mmpack folder, and use the
containing folder as root directory.

Otherwise, clone the project from given git url.
If a tag was specified, use it (checkout on the given tag).

If a prefix is given, work within it instead.

Examples:
# From any subfolder of the project
$ mmpack pkg-create

# From anywhere
$ mmpack pkg-create --url ssh://git@intranet.mindmaze.ch:7999/~user/XXX.git --tag v1.0.0-custom-tag-target

optional arguments:
  -h, --help            show this help message and exit
  -s URL, --url URL     project git url
  -t TAG, --tag TAG     project tag
  -p PREFIX, --prefix PREFIX
                        prefix within which to work
  --skip-build-tests    indicate that build tests must not be run
```

## design decisions
* Targets *debian* and *windows*
* Python3
* All files will be in **yaml** if in any format
* Python **MUST** comply with pycodestyle, and *SHOULD* comply with pylint
* Use pre-defined package build tree (rpmbuild style)
* Use debian way of handling symbols and dependencies
* Do not handle any system packages (but have dependencies upon them).
  Should one be needed and not installed, abort and request its installation
* Use python unittest module (not pytest)

# Planned work
* Build server for cross compilation
* Create new package from previous one
* Add reference mindmaze repository
* Add mmpack *update* command
