mmpack_repo_private = files(
  '__init__.py',
  '__main__.py',
  'repo.py',
)
all_sources += mmpack_repo_private

mmpack_repo_scripts = configure_file(
	output: 'mmpack-modifyrepo',
	input: files('mmpack-modifyrepo.py.in'),
	configuration: config,
)
all_sources += files('mmpack-modifyrepo.py.in')

install_data(mmpack_repo_scripts,
        install_dir : bindir,
)

install_data(mmpack_repo_private,
        install_dir : pkgdatadir / 'repository',
)
