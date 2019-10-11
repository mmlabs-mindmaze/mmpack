cc.check_header('check.h')
libcheck = dependency('check', required : true)
unittest_args = [
    '-DSRCDIR="' + meson.source_root() + '"',
    '-DBUILDDIR="' + meson.build_root() + '"',
]

mmpack_unit_tests_sources = files(
        'binindex_test.c',
        'config_test.c',
        'dependencies_test.c',
        'indextable_test.c',
        'misc_test.c',
        'sha_test.c',
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

# double default timeout (30s) because windows platforms are slower
test('mmpack unit tests',
        mmpack_unit_tests,
        timeout : 60,
        suite : 'mmpack',
)

mmpack_build_unit_tests_sources = files(
    'binary-indexes/circular.yaml',
    'binary-indexes/complex-dependency.yaml',
    'binary-indexes/installed-simple.yaml',
    'binary-indexes/simplest.yaml',
    'binary-indexes/simple.yaml',
    'binary-indexes/unsolvable-dependencies.yaml',
    'mmpack-config.yaml',
    'pytests.py',
    'specfiles/full.yaml',
    'specfiles/simple.yaml',
    'specfiles/simple.yaml',
    'test_package.py',
    'test_version.py',
)

env = environment()
env.set('DEPLOYMENT_DIR', deployment_dir)
env.set('PYTHON_INSTALL_DIR', python.get_install_dir())
env.set('SRCDIR', meson.source_root())
env.set('BUILDDIR', meson.build_root())
env.set('PREFIX', get_option('prefix'))

test('mmpack-build unit tests',
        files('python-unittests.tap'),
        suite : 'mmpack-build',
        env : env,
        depends : do_deployment_test,
)

# The variable _MMPACK_TEST_PREFIX is needed by mmpack-build and mmpack to find
# everything in the right place during the tests (that require a local
# installation of mmpack-build and mmpack).
test('mmpack-build',
        files ('smoke-tests/test_mmpack-build.sh'),
        timeout : 60,
        env : env,
        suite : 'mmpack-build-smoke',
)

mmpack_smoke_test = [
        ['install', files('smoke-tests/test_mmpack_install.sh')],
        ['remove', files('smoke-tests/test_mmpack_remove.sh')],
        ['check-integrity', files('smoke-tests/test_mmpack_check-integrity.sh')],
]

foreach test : mmpack_smoke_test
        test('mmpack-smoke-' + test[0],
                test[1],
                env : env,
                suite : 'mmpack-smoke',
        )
endforeach
