subdir('detours')

mmpack_lib_sources += files(
	'exec-in-prefix.c',
        'path-win32.c',
        'path-win32.h',
)


internal_ntdll = custom_target('internal_ntdll',
        output: 'internal-ntdll.dll.a',
        input: 'internal-ntdll.def',
        command: [dlltool, '-d', '@INPUT@', '-l', '@OUTPUT@'],
)


mount_helper_lib_sources = files(
    'api-hook.c',
    'mmpack-mount-prefix.h',
    'mount-helper.c',
    'mount-helper.h',
)
all_c_sources += mount_helper_lib_sources

deployed_targets += shared_module('mount-mmpack-prefix',
        mount_helper_lib_sources + ['internal-ntdll.h'],
        include_directories : inc,
        install : true,
        install_dir : get_option('libexecdir') / 'mmpack',
        link_with: [detours, internal_ntdll],
        link_args: ['-static'],
)
