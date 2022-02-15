mmpack_build_completion = configure_file(
    output : 'mmpack-build',
    input : 'shell-completions/bash/mmpack-build.in',
    configuration : config,
)

install_data(
    files(
        'shell-completions/bash/mmpack',
    ) + [
        mmpack_build_completion
    ],
    install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)
