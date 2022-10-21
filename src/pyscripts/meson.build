pyscripts_sources = files(
	'__init__.py',
	'__main__.py',
        'depends.py',
        'dispatch.py',
        'provides.py',
	'utils.py',
)
all_sources += pyscripts_sources

pyscripts_installdir = pkgdatadir / 'pyscripts'

install_data(pyscripts_sources, install_dir : pyscripts_installdir)

if not get_option('no-pypkg-vendor')
  subdir('vendor')
else
  python3_required_modules = ['astroid']
endif
