# patchelf is required to create any package containing elf files
if host_machine.system() == 'linux'
    find_program('patchelf', required : true)
endif

mmpack_build_sources = files(
        '__init__.py',
        '__main__.py',
        'base_hook.py',
        'binary_package.py',
        'common.py',
        'decorators.py',
        'elf_utils.py',
        'file_utils.py',
        'hook_locales.py',
        'hook_python.py',
        'hook_sharedlib.py',
        'hooks_loader.py',
        'mmpack_builddep.py',
        'mmpack_clean.py',
        'mmpack_guess.py',
        'mmpack_pkg_create.py',
        'pe_utils.py',
        'provide.py',
        'python_depends.py',
        'python_provides.py',
        'source_tarball.py',
        'src_package.py',
        'syspkg_manager.py',
        'syspkg_manager_base.py',
        'syspkg_manager_dpkg.py',
        'syspkg_manager_pacman_msys2.py',
        'mm_version.py',
        'workspace.py',
        'xdg.py',
)

config_data = configuration_data({
    'bindir' :  get_option('prefix') + '/' + get_option('bindir'),
    'libexecdir' : get_option('prefix') + '/' + get_option('libexecdir'),
    'mmpackbuilddir' : get_option('prefix') + '/' + get_option('datadir') + '/mmpack/python',
    'pkgdatadir' : get_option('prefix') + '/' + get_option('datadir') + '/mmpack',
    'python_install_dir' : python.get_install_dir(),
    'package_version' : meson.project_version(),
    'exeext' : exeext,
})

settings_py = configure_file(
        output : 'settings.py',
        input : files('settings.py.in'),
        configuration : config_data,
)
mmpack_build_sources += settings_py

python.install_sources(mmpack_build_sources, subdir : 'mmpack_build')

mmpack_build = configure_file(
        output : 'mmpack-build',
        input : 'mmpack-build.in',
        configuration : config_data,
)
install_data(mmpack_build,
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
