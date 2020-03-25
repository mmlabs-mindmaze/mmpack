install_data(files('mmpack-build-slave.sh'),
        install_dir : get_option('bindir'),
        install_mode: 'rwxr-xr-x'
)

ci_builder_files = files(
        'main.py',
        'gerrit.py',
)
install_data(ci_builder_files,
    install_dir : get_option('datadir') / 'mmpack/ci-builder',
)
