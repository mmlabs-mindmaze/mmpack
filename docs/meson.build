man_pages_sources = [
    'man/mmpack.rst',
    'man/mmpack-autoremove.rst',
    'man/mmpack-build.rst',
    'man/mmpack-build-builddep.rst',
    'man/mmpack-build-clean.rst',
    'man/mmpack-build-guess.rst',
    'man/mmpack-build-pkg-create.rst',
    'man/mmpack-check-integrity.rst',
    'man/mmpack-download.rst',
    'man/mmpack-fix-broken.rst',
    'man/mmpack-install.rst',
    'man/mmpack-list.rst',
    'man/mmpack-mkprefix.rst',
    'man/mmpack-provides.rst',
    'man/mmpack-rdepends.rst',
    'man/mmpack-remove.rst',
    'man/mmpack-repo.rst',
    'man/mmpack-run.rst',
    'man/mmpack-search.rst',
    'man/mmpack-show.rst',
    'man/mmpack-source.rst',
    'man/mmpack-update.rst',
    'man/mmpack-upgrade.rst',
]

# Note:
# This is not yet supported by meson, but is the future way this should be done
# https://github.com/mesonbuild/meson/issues/3206
#
# man1 = generator(rst2man,
#         output : '@BASENAME@.1',
#         arguments : ['@INPUT@', '@OUTPUT@']
# )
# man_pages = man1.process(rstfiles)
# install_data(man_pages)

rst2man = find_program(['rst2man', 'rst2man3'], required : true)
foreach rstfile : man_pages_sources
    custom_target(rstfile.underscorify(),
        command : [rst2man, files(rstfile)],
        output : '@BASENAME@.1',
        capture : '@BASENAME@.1',
        input : rstfile,
        build_by_default : true,
        install : true,
        install_dir : get_option('mandir') / 'man1',
    )
endforeach

sphinxbuild = find_program('sphinx-build', required : true)
mmpack_doc_sources = files(
    'bug.rst',
    'configuration.rst',
    'conf.py',
    'creating-packages.rst',
    'devguide.rst',
    'faq.rst',
    'getting-started.rst',
    'index.rst',
    'installation.rst',
    'introduction.rst',
    'usage.rst',
    'yaml.rst',
)
# add the user part of the specifications
mmpack_doc_sources += files(
  'specs/package-sources-strap.rst',
  'specs/package-specfile.rst',
)

custom_target('html',
        output : 'html',
        command : [
            sphinxbuild,
            '-b', 'html',
            '-d', 'docs/.doctrees',
            '-j', 'auto',
            '-D', 'version=' + meson.project_version(),
            meson.source_root() / 'docs',
            'docs/html',
        ],
        build_by_default : true,
        depend_files : files('conf.py'),
        install : true,
        install_dir : get_option('datadir') / 'doc/mmpack',
)
