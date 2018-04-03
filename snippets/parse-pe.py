import sys
import os
import pefile

filename = sys.argv[1]

d = [pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]]
pe = pefile.PE(filename, fast_load=True)
pe.parse_data_directories(directories=d)

exports = [e.name for e in pe.DIRECTORY_ENTRY_EXPORT.symbols]
for export in sorted(exports):
    print(export.decode('utf-8'))
