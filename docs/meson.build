man_pages_sources = [
    'man/1/mmpack.rst',
    'man/1/mmpack-autoremove.rst',
    'man/1/mmpack-build.rst',
    'man/1/mmpack-build-builddep.rst',
    'man/1/mmpack-build-clean.rst',
    'man/1/mmpack-build-guess.rst',
    'man/1/mmpack-build-pkg-create.rst',
    'man/1/mmpack-check-integrity.rst',
    'man/1/mmpack-download.rst',
    'man/1/mmpack-fix-broken.rst',
    'man/1/mmpack-install.rst',
    'man/1/mmpack-list.rst',
    'man/1/mmpack-mkprefix.rst',
    'man/1/mmpack-provides.rst',
    'man/1/mmpack-rdepends.rst',
    'man/1/mmpack-remove.rst',
    'man/1/mmpack-repo.rst',
    'man/1/mmpack-run.rst',
    'man/1/mmpack-search.rst',
    'man/1/mmpack-show.rst',
    'man/1/mmpack-source.rst',
    'man/1/mmpack-update.rst',
    'man/1/mmpack-upgrade.rst',
    'man/5/configuration.rst',
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
    man_section = rstfile.split('/')[1]
    custom_target(rstfile.underscorify(),
        command : [rst2man, files(rstfile)],
        output : '@BASENAME@.' + man_section,
        capture : '@BASENAME@.' + man_section,
        input : rstfile,
        build_by_default : true,
        install : true,
        install_dir : get_option('mandir') / 'man' + man_section,
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
