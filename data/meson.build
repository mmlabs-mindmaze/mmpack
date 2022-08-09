install_data(
    files(
        'shell-completions/bash/mmpack',
        'shell-completions/bash/mmpack-build',
        'shell-completions/bash/mmpack-modifyrepo',
    ),
    install_dir : get_option('datadir') / 'bash-completion' / 'completions',
)

licenses = files(
    'common-licenses/Apache-2.0',
    'common-licenses/Artistic',
    'common-licenses/BSD',
    'common-licenses/CC0-1.0',
    'common-licenses/GFDL-1.2',
    'common-licenses/GFDL-1.3',
    'common-licenses/GPL-1',
    'common-licenses/GPL-2',
    'common-licenses/GPL-3',
    'common-licenses/LGPL-2',
    'common-licenses/LGPL-2.1',
    'common-licenses/LGPL-3',
    'common-licenses/MPL-1.1',
    'common-licenses/MPL-2.0',
)

install_data(licenses,
    install_dir : get_option('datadir') / meson.project_name() / 'common-licenses'
)
