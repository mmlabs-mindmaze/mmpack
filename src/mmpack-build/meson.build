mmpack_build_sources = files(
        '__init__.py',
        '__main__.py',
        'base_hook.py',
        'binary_package.py',
        'common.py',
        'dpkg.py',
        'decorators.py',
        'elf_utils.py',
        'file_utils.py',
        'hook_python.py',
        'hook_sharedlib.py',
        'hooks_loader.py',
        'mmpack_builddep.py',
        'mmpack_clean.py',
        'mmpack_pkg_create.py',
        'pacman.py',
        'pe_utils.py',
        'provide.py',
        'src_package.py',
        'mm_version.py',
        'workspace.py',
        'xdg.py',
)

config_data = configuration_data({
    'bindir' :  get_option('prefix') + '/' + get_option('bindir'),
    'libexecdir' : get_option('prefix') + '/' + get_option('libexecdir'),
    'mmpackbuilddir' : get_option('prefix') + '/' + get_option('datadir') + '/mmpack/python',
    'pkgdatadir' : get_option('prefix') + '/' + get_option('datadir') + '/mmpack',

    'exeext' : exeext,
})

settings_py = configure_file(
        output : 'settings.py',
        input : files('settings.py.in'),
        configuration : config_data,
)
mmpack_build_sources += settings_py

python.install_sources(mmpack_build_sources, subdir : 'mmpack_build')

install_data(files('mmpack-build'),
    install_dir : get_option('bindir'),
)

mmpack_build_plugins = files(
	'build-autotools',
	'build-cmake',
	'build-makefile',
	'build-meson',
	'build-python',
)
install_data(mmpack_build_plugins,
    install_dir : get_option('datadir') / 'mmpack',
)

install_data(files('mmpack-build_completion'),
        install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)
