mmpack_handle_repo = files('mmpack-createrepo')

mmpack_handle_repo_private = files('repo.py')

conf_data = configuration_data({
	'datadir': get_option('datadir') + '/mmpack',
	'bindir': get_option('bindir'),
})

server_upload_py = configure_file(
	output: 'server_upload',
	input: files('server_upload.py.in'),
	configuration: conf_data,
)
mmpack_handle_repo += server_upload_py

install_data(mmpack_handle_repo,
        install_dir : get_option('bindir'),
        install_mode: 'rwxr-xr-x'
)

install_data(mmpack_handle_repo_private,
        install_dir : get_option('datadir') + '/mmpack',
        install_mode: 'rw-r--r--'
)
