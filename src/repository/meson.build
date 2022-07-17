mmpack_repo_private = files('repo.py')
all_sources += mmpack_repo_private

conf_data = configuration_data({
	'pkgdatadir': get_option('datadir') + '/mmpack',
	'bindir': get_option('bindir'),
})

mmpack_repo_scripts = configure_file(
	output: 'mmpack-modifyrepo',
	input: files('mmpack-modifyrepo.py.in'),
	configuration: conf_data,
)
all_sources += files('mmpack-modifyrepo.py.in')
deployed_targets += mmpack_repo_scripts

install_data(mmpack_repo_scripts,
        install_dir : get_option('bindir'),
)

install_data(mmpack_repo_private,
        install_dir : get_option('datadir') + '/mmpack',
)
