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
 :repositories: a list of url repositories. The url syntax
   follows RFC 3986.
