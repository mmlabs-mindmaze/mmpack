Configuration
=============

The mmpack configuration file is written in yaml.
It is shared between **mmpack** and **mmpack-build**.

The mmpack configuration file that will be used is the current prefix mmpack
configuration file if working within a prefix, or the global (default)
configuration file that is located in **$XDG_CONFIG_HOME** folder (defaults to
$HOME/.config). The mmpack configuration file is called **mmpack-config.yaml**.

There are two configuration options:

 :default-prefix: a path to the prefix to use if none given
 :repositories: a list of repository names. Each repository contains two
     fields named respectively url and enabled. The field url provides the url
     where to find the packages of the repository. The url syntax follows RFC
     3986. The field enabled is a flag either set to 0 or 1: if this flag is
     set to 0 then the repository is disabled, i.e., the user is not able to
     access to the packages of the repository; on the contrary, when this flag
     is set to 1 then the repository is enabled, i.e., the user is able to
     access to the packages of the repository. The enabled flag is optional and
     it defaults to 1.

 Example of list of repositories:


 .. code-block:: yaml

    repositories:
      - repo_name:
            url: url_of_repo
            enabled: 0
      - other_repo_name:
            url: url_of_other_repo
            enabled: 1
