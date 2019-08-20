man_pages_sources = [
    'man/mmpack.rst',
    'man/mmpack-build.rst',
    'man/mmpack-build-builddep.rst',
    'man/mmpack-build-clean.rst',
    'man/mmpack-build-pkg-create.rst',
    'man/mmpack-check-integrity.rst',
    'man/mmpack-download.rst',
    'man/mmpack-fix-broken.rst',
    'man/mmpack-install.rst',
    'man/mmpack-list.rst',
    'man/mmpack-mkprefix.rst',
    'man/mmpack-provides.rst',
    'man/mmpack-remove.rst',
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
