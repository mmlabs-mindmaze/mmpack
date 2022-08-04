long_test = get_option('long-tests') or disabler()

unittest_args = [
    '-DSRCDIR="' + meson.project_source_root() + '"',
    '-DBUILDDIR="' + meson.project_build_root() + '"',
]

mmpack_unit_tests_sources = files(
        'binindex_test.c',
        'config_test.c',
        'dependencies_test.c',
        'indextable_test.c',
        'misc_test.c',
        'sha_test.c',
        'srcindex_test.c',
        'testcases.h',
        'unittests.c',
        'version_test.c',
)
mmpack_unit_tests = executable('mmpack-unit-test',
    mmpack_unit_tests_sources,
    c_args : unittest_args,
    include_directories : include_directories('.', '..', '../src/mmpack'),
    link_with : libmmpack,
    dependencies : [libcheck],
)

# this should take about 12s
# increase timeout to be able to run this
# - on windows (should take about 40s)
# - with address sanitizer enabled (should take 30s)
# (timing are expectations and depend on the platform specs and load)
test('mmpack unit tests',
        mmpack_unit_tests,
        timeout : 60,
        suite : 'mmpack',
)

mmpack_build_unit_tests_sources = files(
    'binary-indexes/circular.gz',
    'binary-indexes/complex-dependency.gz',
    'binary-indexes/installed-simple.gz',
    'binary-indexes/simplest.gz',
    'binary-indexes/simple.gz',
    'binary-indexes/unsolvable-dependencies.gz',
    'mmpack-config.yaml',
    'pydata/bare.py',
    'pydata/multi/__init__.py',
    'pydata/multi/foo.py',
    'pydata/multi/bar.py',
    'pydata/multi/utils.py',
    'pydata/pkg_imported/__init__.py',
    'pydata/simple/__init__.py',
    'pytests.py',
    'source-indexes/multi-repo0',
    'source-indexes/multi-repo1',
    'source-indexes/reference.multi',
    'source-indexes/reference.simple',
    'source-indexes/simple',
    'specfiles/full.yaml',
    'specfiles/simple.yaml',
    'specfiles/simple.yaml',
    'test_common.py',
    'test_file_utils.py',
    'test_hook_python.py',
    'test_package.py',
    'test_readme_parsing.py',
    'test_version.py',
)

# The variable _MMPACK_TEST_PREFIX is needed by mmpack and mmpack-build to find
# everything in the right place during the tests (that require a local
# installation of mmpack project).
env = environment()
env.set('_MMPACK_TEST_PREFIX', deployment_dir)
env.set('PKGDATADIR', get_option('prefix') / get_option('datadir') + '/mmpack')
env.set('SRCDIR', meson.project_source_root())
env.set('BUILDDIR', meson.project_build_root())
env.set('PREFIX', get_option('prefix'))
env.set('REPO', repo_dir)

test('mmpack-build unit tests',
        files('python-unittests.tap'),
        suite : 'mmpack-build',
        env : env,
        depends : [do_deployment_test, do_test_repo],
)

test('mmpack-build-meson',
        files ('smoke-tests/test_mmpack-build.sh'),
        env : env,
        args : 'meson',
        suite : 'mmpack-build-smoke',
        depends : do_test_pkg,
)

if get_option('long-tests')
  test('mmpack-build-autotools',
        files ('smoke-tests/test_mmpack-build.sh'),
        timeout : 120,
        env : env,
        args : 'autotools',
        suite : 'mmpack-build-smoke',
        depends : do_test_pkg,
  )

  test('mmpack-build-ghost',
        files ('smoke-tests/test_mmpack-build_ghost.sh'),
        timeout : 120,
        env : env,
        suite : 'mmpack-build-smoke',
        depends : do_test_sysrepo,
  )
endif

# please keep alphabetically ordered
mmpack_smoke_test = [
        ['autoremove', files('smoke-tests/test_mmpack_autoremove.sh')],
        ['check-integrity', files('smoke-tests/test_mmpack_check-integrity.sh')],
        ['download', files('smoke-tests/test_mmpack_download.sh')],
        ['install', files('smoke-tests/test_mmpack_install.sh')],
        ['list', files('smoke-tests/test_mmpack_list.sh')],
        ['manually_installed', files('smoke-tests/test_mmpack_manually_installed.sh')],
        ['mkprefix', files('smoke-tests/test_mmpack_mkprefix.sh')],
        ['rdepends', files('smoke-tests/test_mmpack_rdepends.sh')],
        ['remove', files('smoke-tests/test_mmpack_remove.sh')],
        ['repo', files('smoke-tests/test_mmpack_repo.sh')],
        ['show', files('smoke-tests/test_mmpack_show.sh')],
        ['search', files('smoke-tests/test_mmpack_search.sh')],
        ['source', files('smoke-tests/test_mmpack_source.sh')],
        ['upgrade', files('smoke-tests/test_mmpack_upgrade.sh')],
]

foreach test : mmpack_smoke_test
        test('mmpack-smoke-' + test[0],
                test[1],
                env : env,
                suite : 'mmpack-smoke',
        )
endforeach
