# @mindmaze_header@
'''
Create a mmpack package

Usage:
mmpack pkg create <git-url> <tag> [options]
'''

import os
import sys

from workspace import Workspace
from common import shell, dprint, iprint
from package import Package, Version


def parse_options(argv):
    'parse options (TODO: real option parsing ...)'
    ctx = {}
    try:
        ctx['url'] = argv[1]
        # tag MUST be of format vx.y.z[-whatever]
        # version numbers will be extracted from its name as x.y.z
        ctx['tag'] = argv[2]  # FIXME: '/' are unsupported within tags
    except IndexError:
        print(__doc__)
        raise

    ctx['version'] = Version(ctx['tag'].split('-')[0][1:])
    ctx['name'] = os.path.basename(ctx['url'])
    if ctx['name'].endswith('.git'):
        ctx['name'] = ctx['name'][:-4]

    return ctx


def main():
    'TODOC'
    ctx = parse_options(sys.argv)

    cmd = r"gpg --list-secret-keys | sed -n 's/.*\[ultimate\] \(.*\)/\1/p'"
    maintainer = shell(cmd)
    dprint('maintainer read from gpg key: ' + maintainer)

    package = Package(ctx['name'], ctx['tag'], ctx['version'],
                      ctx['url'], maintainer)

    wrk = Workspace()
    wrk.clean(ctx['name'])

    src_pkg = package.create_source_archive()
    package.local_install(src_pkg)

    # TODO: ventilate before generating dependencies
    package.gen_dependencies()
    package.gen_provides()
    package.save_metadatas()
    (pkg, doc_pkg, dev_pkg, dbgsym_pkg) = package.build_package()

    iprint('generated package: {}'.format(pkg))
    iprint('generated source package: {}'.format(src_pkg))
    if doc_pkg:
        iprint('generated doc package: {}'.format(doc_pkg))
    if dev_pkg:
        iprint('generated dev package: {}'.format(dev_pkg))
    if dbgsym_pkg:
        iprint('generated dbgsym package: {}'.format(dbgsym_pkg))


if __name__ == '__main__':
    main()
