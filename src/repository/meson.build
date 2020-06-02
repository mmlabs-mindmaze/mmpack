mmpack_repo_scripts = files('mmpack-createrepo')

mmpack_repo_private = files('repo.py')

conf_data = configuration_data({
	'pkgdatadir': get_option('datadir') + '/mmpack',
	'bindir': get_option('bindir'),
})

mmpack_repo_scripts += configure_file(
	output: 'mmpack-repo',
	input: files('mmpack-repo.py.in'),
	configuration: conf_data,
)

install_data(mmpack_repo_scripts,
        install_dir : get_option('bindir'),
        install_mode: 'rwxr-xr-x'
)

install_data(mmpack_repo_private,
        install_dir : get_option('datadir') + '/mmpack',
        install_mode: 'rw-r--r--'
)
