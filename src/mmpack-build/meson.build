mmpack_build_sources = files(
	'binary_package.py',
	'common.py',
	'dpkg.py',
	'decorators.py',
	'elf_utils.py',
	'file_utils.py',
	'mmpack_builddep.py',
	'mmpack_clean.py',
	'mmpack_pkg_create.py',
	'pacman.py',
	'pe_utils.py',
	'python_utils.py',
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

configure_file(
        output : 'settings.py',
        input : files('settings.py.in'),
        configuration : config_data,
        install : true,
        install_dir : get_option('datadir') / 'mmpack' / 'python',
)

configure_file(
        output : 'mmpack-build',
        input : files('mmpack-build.in'),
        configuration : config_data,
        install : true,
        install_dir :  get_option('bindir'),
        install_mode : 'rwxr-xr-x',
)


install_data(mmpack_build_sources,
    install_dir : get_option('datadir') / 'mmpack' / 'python',
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
