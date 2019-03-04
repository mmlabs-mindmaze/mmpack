About the yaml format
=====================

Yaml is the language format chosen for all mmpack interfaces.
It is quite commonly used for configuration files.
It is natural and simple to use

Many reasons support this choice, but mainly:

 * it does the job
 * json syntax does not provide comments
 * xml is heavy as hell


Basic syntax
------------

Use spaces, not tab. Always.

Lists are written either way as follows:

.. code-block:: yaml

   # python-like
   key: [value1, value2, ...]

   # this is interpreted the same
   key:
     - value1
     - value2

Big blocks of text can be declared as follows with a pipe,
and then with indenting the text block


.. code-block:: yaml

   key: |
     beginning of some huge text block ...
     ... continuing ...
     end.


References
----------

* The yaml official web site: https://yaml.org/
