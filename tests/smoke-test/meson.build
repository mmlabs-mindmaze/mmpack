project('hello', 'c',
        version : '1.0.0',
        meson_version: '>= 0.49'
)

add_global_arguments('-DLIBEXECDIR="' + get_option('prefix') / get_option('libexecdir') + '"', language : 'c')
if host_machine.system() == 'windows'
	add_global_arguments('-DEXEEXT=".exe' + '"', language : 'c')
else
	add_global_arguments('-DEXEEXT=', language : 'c')
endif

executable('hello-world',
        'hello-world.c',
        install : true,
        include_directories : include_directories('.'),
)

executable('head-libexec-world',
        'head-libexec-world.c',
        install : true,
        include_directories : include_directories('.'),
)

install_data(files('shell-exec.py'),
        'shell-exec.py',
        install_dir : get_option('bindir'),
)

executable('libexec-world',
        'libexec-world.c',
        install : true,
        include_directories : include_directories('.'),
	install_dir : get_option('libexecdir') / meson.project_name(),
)
