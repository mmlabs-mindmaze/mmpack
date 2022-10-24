mmpack_lib_sources += files(
	'exec-in-prefix.c',
)

mount_mmpack_prefix_sources = files('main.c')
all_c_sources += mount_mmpack_prefix_sources

mount_mmpack_prefix = executable('mount-mmpack-prefix',
        mount_mmpack_prefix_sources,
        include_directories : inc,
        install : true,
        install_dir : pkglibexecdir,
)
deployed_targets += mount_mmpack_prefix

# dev linux-only helper function, not part of any target
# requires root permissions to run
setcap = find_program('/sbin/setcap', required : false)
if setcap.found()
    run_target('setcap',
            command : [
                setcap,
                'cap_dac_override,cap_sys_admin+ep',
                pkglibexecdir / 'mount-mmpack-prefix'
            ],
    )
endif # setcap
