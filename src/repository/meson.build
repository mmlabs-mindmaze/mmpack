mmpack_repo_private = files('repo.py')

conf_data = configuration_data({
	'pkgdatadir': get_option('datadir') + '/mmpack',
	'bindir': get_option('bindir'),
})

mmpack_repo_scripts = configure_file(
	output: 'mmpack-modifyrepo',
	input: files('mmpack-modifyrepo.py.in'),
	configuration: conf_data,
)

install_data(mmpack_repo_scripts,
        install_dir : get_option('bindir'),
)

install_data(mmpack_repo_private,
        install_dir : get_option('datadir') + '/mmpack',
)
