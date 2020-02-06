mmpack_handle_repo = files('mmpack-createrepo')

mmpack_handle_repo_private = files('repo.py')

conf_data = configuration_data({
	'pkgdatadir': get_option('datadir') + '/mmpack',
	'bindir': get_option('bindir'),
})

mmpack_handle_repo_py = configure_file(
	output: 'mmpack_handle_repo',
	input: files('mmpack_handle_repo.py.in'),
	configuration: conf_data,
)
mmpack_handle_repo += mmpack_handle_repo_py

install_data(mmpack_handle_repo,
        install_dir : get_option('bindir'),
        install_mode: 'rwxr-xr-x'
)

install_data(mmpack_handle_repo_private,
        install_dir : get_option('datadir') + '/mmpack',
        install_mode: 'rw-r--r--'
)
