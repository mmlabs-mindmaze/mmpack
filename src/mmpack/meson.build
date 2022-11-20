libcurl = dependency('libcurl', required : true)
libarchive = dependency('libarchive', required : true)
zlib = dependency('zlib', required : true)
yaml = cc.find_library('yaml', required : true)
libmmlib = cc.find_library('mmlib', required : true)
nettle = dependency('nettle', required : true)


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
	'exec-in-prefix.h',
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


# include top dir for config.h header
inc = include_directories('.', '../../')

if host_machine.system() == 'windows'
    subdir('mount-prefix-nt')
else
    subdir('mount-prefix-linux')
endif

gen_site_py = cstrgen.process('site.py')

libmmpack = static_library('mmpack-static',
        mmpack_lib_sources,
        gen_site_py,
        include_directories : inc,
        dependencies: [libcurl, libarchive, yaml, libmmlib, zlib, nettle],
)
all_c_sources += mmpack_lib_sources

mmpack_sources = files('mmpack.c')
mmpack = executable('mmpack',
        mmpack_sources,
        include_directories : inc,
        link_with : libmmpack,
        install : true,
)
all_c_sources += mmpack_sources
deployed_targets += mmpack

install_data(
        files(
          'prompt.bash',
          'check-dpkg-installed',
        ),
        install_dir : pkgdatadir,
)
