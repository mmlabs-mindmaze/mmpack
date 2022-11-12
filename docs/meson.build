man_pages_sources = [
    'man/1/mmpack.rst',
    'man/1/mmpack_autoremove.rst',
    'man/1/mmpack-build.rst',
    'man/1/mmpack-build_builddep.rst',
    'man/1/mmpack-build_clean.rst',
    'man/1/mmpack-build_guess.rst',
    'man/1/mmpack-build_mksource.rst',
    'man/1/mmpack-build_pkg-create.rst',
    'man/1/mmpack_check-integrity.rst',
    'man/1/mmpack_download.rst',
    'man/1/mmpack_fix-broken.rst',
    'man/1/mmpack_install.rst',
    'man/1/mmpack_list.rst',
    'man/1/mmpack_mkprefix.rst',
    'man/1/mmpack-modifyrepo.rst',
    'man/1/mmpack_provides.rst',
    'man/1/mmpack_rdepends.rst',
    'man/1/mmpack_remove.rst',
    'man/1/mmpack_repo.rst',
    'man/1/mmpack_run.rst',
    'man/1/mmpack_search.rst',
    'man/1/mmpack_show.rst',
    'man/1/mmpack_source.rst',
    'man/1/mmpack_update.rst',
    'man/1/mmpack_upgrade.rst',
    'man/5/mmpack-config.rst',
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
        capture : true,
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
  'specs/repo.rst',
)

custom_target('html',
        output : 'html',
        command : [
            sphinxbuild,
            '-q',
            '-b', 'html',
            '-d', 'docs/.doctrees',
            '-j', 'auto',
            '-D', 'version=' + meson.project_version(),
            meson.current_source_dir(),
            '@OUTPUT@',
        ],
        build_by_default : true,
        depend_files : files('conf.py'),
        install : true,
        install_dir : get_option('datadir') / 'doc/mmpack',
)
