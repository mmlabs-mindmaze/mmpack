mount_mmpack_prefix_sources = files('main.c')

mount_mmpack_prefix = executable('mount-mmpack-prefix',
        mount_mmpack_prefix_sources,
        include_directories : inc,
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
)


