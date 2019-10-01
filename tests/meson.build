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
custom_target('mmpack-build-unit-tests',
        command : [ 'PYTHONPATH=' + meson.source_root() + '/src/mmpack-build'
                                  + ':' +  meson.build_root(),
                    files('pytests.py')],
        output : '.pytests.out',
        build_always_stale : true,
        console : true,
        depend_files : mmpack_build_unit_tests_sources,
)

test('mmpack-build unit tests',
        files('python-unittests.tap'),
        env : ['srcdir=' + meson.source_root(),
               'builddir=' + meson.build_root()],
)
