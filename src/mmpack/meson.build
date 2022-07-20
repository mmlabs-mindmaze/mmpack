mmpack_lib_sources = files(
	'action-solver.c',
	'action-solver.h',
	'binindex.c',
	'binindex.h',
	'binpkg.c',
	'binpkg.h',
	'buffer.c',
	'buffer.h',
	'cmdline.c',
	'cmdline.h',
	'common.h',
	'constraints.h',
	'context.c',
	'context.h',
	'download.c',
	'download.h',
	'indextable.c',
	'indextable.h',
	'install-state.c',
	'install-state.h',
	'mmpack-autoremove.c',
	'mmpack-autoremove.h',
	'mmpack-check-integrity.c',
	'mmpack-check-integrity.h',
	'mmpack-check-sysdep.c',
	'mmpack-check-sysdep.h',
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
	'mmpack-rdepends.c',
	'mmpack-rdepends.h',
	'mmpack-remove.c',
	'mmpack-remove.h',
	'mmpack-repo.c',
	'mmpack-repo.h',
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
	'repo.c',
	'repo.h',
	'settings.c',
	'settings.h',
	'sha256.c',
	'sha256.h',
	'srcindex.c',
	'srcindex.h',
	'strchunk.h',
	'strchunk.c',
	'strlist.h',
	'strlist.c',
	'sysdeps.c',
	'sysdeps.h',
	'tar.c',
	'tar.h',
	'utils.c',
	'utils.h',
	'xx-alloc.h',
)
if host_machine.system() == 'windows'
    mmpack_lib_sources += files(
            'path-win32.c',
            'path-win32.h',
    )
endif

all_c_sources += mmpack_lib_sources

# include top dir for config.h header
inc = include_directories('.', '../../')

gen_site_py = cstrgen.process('site.py')

libmmpack = static_library('mmpack-static',
        mmpack_lib_sources,
        gen_site_py,
        include_directories : inc,
        dependencies: [libcurl, libarchive, libyaml, libmmlib, zlib],
)

mmpack_sources = files('mmpack.c')
mmpack = executable('mmpack',
        mmpack_sources,
        include_directories : inc,
        link_with : libmmpack,
        install : true,
        dependencies: [libcurl, libarchive, libyaml, libmmlib, zlib],
)
all_c_sources += mmpack_sources

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
all_c_sources += mount_mmpack_prefix_sources


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
