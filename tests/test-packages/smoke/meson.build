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

hello = shared_library('hello',
        files('libhello.c', 'libhello.h'),
        install : true,
        version : '1.0.0',
)
install_headers(files('libhello.h'))

executable('hello-world',
        'hello-world.c',
        install : true,
        include_directories : include_directories('.'),
        link_with : hello,
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

install_data(files('afile'),
        install_dir : get_option('datadir') / meson.project_name() / '.hidden',
)

executable('libexec-world',
        'libexec-world.c',
        install : true,
        include_directories : include_directories('.'),
	install_dir : get_option('libexecdir') / meson.project_name(),
)
