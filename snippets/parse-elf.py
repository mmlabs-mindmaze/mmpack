from elftools.elf.elffile import ELFFile
from elftools.elf.dynamic import DynamicSection

filename = "libmmlib.so.0.3.4"
f = open(filename, 'rb')
elffile = ELFFile(f)

def remove_duplicates(l):
    for e in l:
        while not e or l.count(e) > 1:
            l.remove(e)

# # list all symbols
# dyn = elffile.get_section_by_name('.dynsym')
# for s in dyn.iter_symbols():
#     if s['st_info']['bind'] == 'STB_GLOBAL' and s['st_other']['visibility'] == 'STV_PROTECTED':
#         print(s.__dict__['name'])

# list all libraries
# (objdump -p $lib | grep NEEDED)
librarylist = []
for section in elffile.iter_sections():
    if isinstance(section, DynamicSection):
        for tag in section.iter_tags():
            if tag.entry.d_tag == 'DT_NEEDED':
                librarylist.append(tag.needed)

print('Shared librares: ', ', '.join(librarylist))

# translate to packages
# (dpkg --search <lib.so>)
import subprocess
packagelist = []
for lib in librarylist:
    cmd = ['dpkg', '--search'] + [lib]
    r = subprocess.run(cmd, check=False, stdout=subprocess.PIPE)
    if r.returncode == 0:
        for pkg in r.stdout.decode('utf-8').split('\n'):
            if pkg.split(':')[0]:
                packagelist.append(pkg.split(':')[0])

remove_duplicates(packagelist)
print('Package list: ', ', '.join(packagelist))
