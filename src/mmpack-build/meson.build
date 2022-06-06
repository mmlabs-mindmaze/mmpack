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
        'errors.py',
        'file_utils.py',
        'hook_locales.py',
        'hook_python.py',
        'hook_sharedlib.py',
        'hooks_loader.py',
        'mmpack_builddep.py',
        'mmpack_clean.py',
        'mmpack_guess.py',
        'mmpack_mksource.py',
        'mmpack_pkg_create.py',
        'package_info.py',
        'pe_utils.py',
        'prefix.py',
        'provide.py',
        'python_depends.py',
        'python_dispatch.py',
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
        'yaml_dumper.py',
)

bindir = get_option('prefix') / get_option('bindir')
pkgdatadir = get_option('prefix') / get_option('datadir') + '/mmpack'
libexecdir = get_option('prefix') / get_option('libexecdir')
mmpackbuilddir = pkgdatadir / 'mmpack_build'

r = run_command('realpath', '-m', '--relative-to=' + bindir, pkgdatadir)
bindir_to_pkgdatadir = r.stdout().strip()

r = run_command('realpath', '-m', '--relative-to=' + mmpackbuilddir, bindir)
pydir_to_bindir = r.stdout().strip()

r = run_command('realpath', '-m', '--relative-to=' + mmpackbuilddir, libexecdir)
pydir_to_libexecdir = r.stdout().strip()

r = run_command('realpath', '-m', '--relative-to=' + mmpackbuilddir, pkgdatadir)
pydir_to_pkgdatadir = r.stdout().strip()

config_data = configuration_data({
    'bindir' : bindir,
    'pydir_to_bindir' :  pydir_to_bindir,
    'pydir_to_libexecdir' : pydir_to_libexecdir,
    'pydir_to_pkgdatadir' : pydir_to_pkgdatadir,
    'bindir_to_pkgdatadir' : bindir_to_pkgdatadir,
    'package_version' : meson.project_version(),
    'exeext' : exeext,
    'python_path' : python.path(),
})

settings_py = configure_file(
        output : 'settings.py',
        input : files('settings.py.in'),
        configuration : config_data,
)
mmpack_build_sources += settings_py

install_data(mmpack_build_sources, install_dir : mmpackbuilddir)

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
    install_dir : mmpackbuilddir + '/buildscripts',
)
