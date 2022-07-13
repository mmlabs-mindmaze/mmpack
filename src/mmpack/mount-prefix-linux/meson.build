mmpack_lib_sources += files(
	'exec-in-prefix.c',
)

mount_mmpack_prefix_sources = files('main.c')

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
