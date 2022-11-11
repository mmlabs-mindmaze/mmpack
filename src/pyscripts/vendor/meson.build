install_subdir('astroid/astroid',
  install_dir: pyscripts_installdir / '_vendor',
  exclude_directories: [
    '__pycache__',
    'interpreter/_import/__pycache__',
    'interpreter/__pycache__',
    'brain/__pycache__',
  ],
)
python3_required_modules += ['lazy_object_proxy', 'wrapt', 'six', 'setuptools']

py_test_paths += meson.current_source_dir() / 'astroid'


install_data(
  files('typing_extensions/src/typing_extensions.py'),
  install_dir: pyscripts_installdir / '_vendor',
)

py_test_paths += meson.current_source_dir() / 'typing_extensions' / 'src'
