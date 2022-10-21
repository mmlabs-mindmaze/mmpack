install_subdir('astroid/astroid',
  install_dir: pyscripts_installdir / '_vendor',
  exclude_directories: [
    '__pycache__',
    'interpreter/_import/__pycache__',
    'interpreter/__pycache__',
    'brain/__pycache__',
  ],
)
py_test_paths += meson.current_source_dir() / 'astroid'
