#!/usr/bin/env python3

import tarfile

# Open archive for reading
tarfile.is_tarfile('/tmp/mmpack.tar')
a = tarfile.open('/tmp/mmpack.tar', 'r')  # 'r:gz' for reading with gzip compression

# a list of all it contains
a.getmembers()
a.getnames()

# get *one* file from the archive
f = a.extractfile('./mmpack-config.py') 
f.read()  # and read it
