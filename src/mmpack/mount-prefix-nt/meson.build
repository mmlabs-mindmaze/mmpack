subdir('detours')

mmpack_lib_sources += files(
	'exec-in-prefix.c',
        'path-win32.c',
        'path-win32.h',
)

shared_library('mmpack-mount-helper',
        files('mount-helper.c'),
        include_directories : inc,
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
        link_with: [detours],
)
