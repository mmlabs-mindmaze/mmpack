# **mmpack** package manager

**mmpack** is a cross-platform package manager.

It is designed to work without any need for root access, and to allow multiple
coexisting project versions within project prefixes (virtualenv-like sandboxes).

If you have questions, requests, issues, pull-requests ...
Please email me: `gabriel.ganne@mindmaze.ch`

## goal

* Allow simpler package creation
* Help when multiple unstable code versions must coexist
* Help for demo packaging and deployment
* Help with cross-platform development

## build and install

**mmpack** depends on *mmlib*, *libcurl* and *libyaml*

**mmpack-build** depends on the following python3 modules: *elftools*, *pefile*, *yaml*

## usage

#### *mmpack* examples

``` bash
# define a prefix to use throughout the whole session
export MMPACK_PREFIX=/custom/prefix/folder

# create a working prefix
mmpack mkprefix --url=http://repository/url $MMPACK_PREFIX

# update package list from given repository
mmpack update

# search for package name
mmpack search name

# install <package-name>
mmpack install package-name
```

#### package creation examples
``` bash
# create mmpack package from local sources during development on a branch named *feature*
mmpack-build pkg-create --url=/path/to/sources --tag=feature

# same but also skipping the tests
mmpack-build pkg-create --url=/path/to/sources --tag=feature --skip-build-tests

# create arbitrary project on tag v1.2.3 from git server
mmpack-build pkg-create --url=ssh://git@intranet.mindmaze.ch:7999/project.git --tag=v1.2.3
```

## Contribute

Sources are hosted on gerrit: https://intranet.mindmaze.ch/mmlabs/gerrit/admin/projects/mmpack

On debian, the *git-review* can ease interactions with gerrit by introducing the
*git review* command.

All pull-requests are automatically tested on both windows and debian.
Python files will go through pylint and pycodestyle. Unit tests will be run.

All contributions are welcome !

## Planned work
* Build server for cross compilation
* Create new package from previous one
* Add mmpack *upgrade* command
