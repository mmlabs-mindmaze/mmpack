install_data(files('mmpack-build-slave.sh'),
        install_dir : get_option('bindir'),
        install_mode: 'rwxr-xr-x'
)

mmpack_build_gerrit_files = files(
        'gerrit/gerrit-mmpack-build.py',
        'gerrit/gerrit.py',
)
install_data(mmpack_build_gerrit_files,
    install_dir : get_option('datadir') / 'mmpack/gerrit/python',
)


#
# dev targets
#

all_c_sources = (mmpack_sources
        + mmpack_lib_sources
        + mmpack_check_sysdep_sources
        + mount_mmpack_prefix_sources)

all_sources = (all_c_sources
        + mmpack_build_sources
        + mmpack_build_plugins)


if get_option('tests')
    all_sources += mmpack_unit_tests_sources
endif

if get_option('doc')
    all_sources += man_pages_sources
endif

codespell = find_program('codespell', required : false)
if codespell.found()
    run_target('codespell',
        command : [
            codespell,
            all_sources,
        ]
    )
endif # codespell

pylint = find_program(['pylint3', 'pylint'], required : false)
if pylint.found()
    run_target('pylint',
        command : [
            pylint,
            '--jobs=0',
            '--disable=locally-disabled',
            '--rcfile', files('pylint-config'),
            deployment_dir + python.get_install_dir() + '/mmpack_build',
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
            deployment_dir,
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
