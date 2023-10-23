find_program('zstd', required : true)

# patchelf is required to create any package containing elf files
if host_machine.system() == 'linux'
    find_program('patchelf', required : true)
endif


python3_required_modules += ['urllib3']
if host_machine.system() == 'windows'
    python3_required_modules += ['pefile']
elif host_machine.system() == 'linux'
    python3_required_modules += ['elftools']
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
        'source_strap_specs.py',
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
all_sources += mmpack_build_sources

mmpackbuilddir = pkgdatadir / 'mmpack_build'

install_data(mmpack_build_sources, install_dir : mmpackbuilddir)

mmpack_build = configure_file(
        output : 'mmpack-build',
        input : 'mmpack-build.in',
        configuration : config,
)
install_data(mmpack_build,
    install_dir : bindir,
)

install_data(
  files(
    'buildscripts/autotools',
    'buildscripts/cmake',
    'buildscripts/makefile',
    'buildscripts/meson',
    'buildscripts/python',
  ),
  install_dir: mmpackbuilddir / 'buildscripts',
)

install_data(files('../../VERSION'),
  install_dir: mmpackbuilddir
)
