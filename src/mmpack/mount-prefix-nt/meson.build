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
