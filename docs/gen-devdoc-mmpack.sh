#!/bin/sh

set -e

# header
cat << EOF
mmpack internal documentation
*****************************

EOF

# mmpack sources
cat << EOF
mmpack
======

EOF
for path in $@ ; do
	echo "   " ".. kernel-doc:: $(readlink -e $path)"
	echo "   " "   :export:"
	echo
done

# mmpack-build sources
cat << EOF
.. include:: modules.rst

EOF


# footer
cat << EOF
Indices and tables
==================

* :ref:\`genindex\`
* :ref:\`modindex\`
* :ref:\`search\`
EOF
