mmpack_lib_sources += files(
	'exec-in-prefix.c',
        'path-win32.c',
        'path-win32.h',
)

mount_mmpack_prefix_sources = files('main.c')
all_c_sources += mount_mmpack_prefix_sources

mount_mmpack_prefix = executable('mount-mmpack-prefix',
        mount_mmpack_prefix_sources,
        include_directories : inc,
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
)


