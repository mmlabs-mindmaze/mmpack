mmpack_lib_sources = files(
	'action-solver.c',
	'action-solver.h',
	'cmdline.c',
	'cmdline.h',
	'common.h',
	'context.c',
	'context.h',
	'download.c',
	'download.h',
	'indextable.c',
	'indextable.h',
	'mm-alloc.h',
	'mmpack-check-integrity.c',
	'mmpack-check-integrity.h',
	'mmpack-download.c',
	'mmpack-download.h',
	'mmpack-fix-broken.c',
	'mmpack-fix-broken.h',
	'mmpack-install.c',
	'mmpack-install.h',
	'mmpack-list.c',
	'mmpack-list.h',
	'mmpack-mkprefix.c',
	'mmpack-mkprefix.h',
	'mmpack-provides.c',
	'mmpack-provides.h',
	'mmpack-remove.c',
	'mmpack-remove.h',
	'mmpack-run.c',
	'mmpack-run.h',
	'mmpack-search.c',
	'mmpack-search.h',
	'mmpack-show.c',
	'mmpack-show.h',
	'mmpack-source.c',
	'mmpack-source.h',
	'mmpack-update.c',
	'mmpack-update.h',
	'mmpack-upgrade.c',
	'mmpack-upgrade.h',
	'mmstring.h',
	'package-utils.c',
	'package-utils.h',
	'pkg-fs-utils.c',
	'pkg-fs-utils.h',
	'settings.c',
	'settings.h',
	'sha256.c',
	'sha256.h',
	'sysdeps.c',
	'sysdeps.h',
	'utils.c',
	'utils.h',
)
if host_machine.system() == 'windows'
    mmpack_lib_sources += files(
            'path-win32.c',
            'path-win32.h',
    )
endif

# include top dir for config.h header
inc = include_directories('.', '../../')

libmmpack = static_library('mmpack-static',
        mmpack_lib_sources,
        include_directories : inc,
        dependencies: [libcurl, libarchive, libyaml, libmmlib],
)

mmpack_sources = files('mmpack.c')
mmpack = executable('mmpack',
        mmpack_sources,
        include_directories : inc,
        link_with : libmmpack,
        install : true,
        dependencies: [libcurl, libarchive, libyaml, libmmlib],
)

mmpack_check_sysdep_sources = files(
	'indextable.h',
	'indextable.c',
	'mmpack-check-sysdep.c',
	'sha256.c',
	'sysdeps.h',
	'sysdeps.c',
	'utils.c',
)
mmpack_check_sysdep = executable('mmpack-check-sysdep',
        mmpack_check_sysdep_sources,
        include_directories : inc,
        dependencies : [libmmlib],
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
)

install_data(files('mmpack_completion'),
        install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)

if host_machine.system() == 'windows'
    mount_mmpack_prefix_sources = files('mount-prefix-nt.c')
else
    mount_mmpack_prefix_sources = files('mount-prefix-linux.c')
endif
mount_mmpack_prefix = executable('mount-mmpack-prefix',
        mount_mmpack_prefix_sources,
        include_directories : inc,
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
)

# dev linux-only helper function, not part of any target
# requires root permissions to run
setcap = find_program('/sbin/setcap', required : false)
if setcap.found()
    run_target('setcap',
            command : [
                setcap,
                'cap_dac_override,cap_sys_admin+ep',
                get_option('prefix') / get_option('libexecdir') / 'mmpack' / 'mount-mmpack-prefix'
            ],
    )
endif # setcap

install_data(files('prompt.bash'),
        install_dir : get_option('datadir') / 'mmpack',
)

install_data(files('check-dpkg-installed'),
        install_dir : get_option('datadir') / 'mmpack',
)
