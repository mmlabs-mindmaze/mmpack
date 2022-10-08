pyscripts_sources = files(
	'__main__.py',
        'depends.py',
        'dispatch.py',
        'provides.py',
	'utils.py',
)
all_sources += pyscripts_sources

pyscripts_installdir = pkgdatadir / 'pyscripts'

install_data(pyscripts_sources, install_dir : pyscripts_installdir)
