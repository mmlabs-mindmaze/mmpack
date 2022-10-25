#
# dev targets
#

if tests_state == 'enabled'
    all_sources += mmpack_unit_tests_sources
endif

if docs_state == 'enabled'
    all_sources += man_pages_sources
endif

codespell = find_program('codespell', required : false)
if codespell.found()
    run_target('codespell',
        command : [
            codespell,
            '-x', files('codespell-ignore'),
            all_sources,
        ]
    )
endif # codespell

pkgdatadir = get_option('prefix') / get_option('datadir') + '/mmpack'
srcdir = meson.project_source_root() / 'src'

pylint = find_program(['pylint3', 'pylint'], required : false)
if pylint.found()
    run_target('pylint',
        command : [
            pylint,
            '--jobs=0',
            '--disable=locally-disabled',
            '--rcfile', files('pylint-config'),
            srcdir / 'mmpack-build',
            srcdir / 'pyscripts',
            srcdir / 'repository',
        ],
    )
endif # pylint

pycodestyle = find_program('pycodestyle', required : false)
if pycodestyle.found()
     run_pycodestyle = custom_target('pycodestyle',
        output : 'pycodestyle',
        command : [
            pycodestyle,
            '--show-source',
            srcdir / 'mmpack-build',
            srcdir / 'pyscripts',
            srcdir / 'repository',
        ],
    )
else
    run_pycodestyle = []
endif # pycodestyle

uncrustify = find_program('uncrustify', required : false)
if uncrustify.found()
    uncrustify_config = files('uncrustify.cfg')
    custom_target('checkstyle',
        output : 'checkstyle',
        command : [
            uncrustify,
            '-l', 'c',
            '-c', uncrustify_config,
            '--check',
            all_c_sources,
        ],
        depends : run_pycodestyle
    )
    run_target('fix-c-style',
        command : [
            uncrustify,
            '-l', 'c',
            '-c', uncrustify_config,
            '--no-backup',
            '--replace',
            all_c_sources,
        ],
    )
endif # uncrustify


meson.add_dist_script('expand_headers')
