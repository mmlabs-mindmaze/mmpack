project('mmpack', ['c', 'cpp'],
        version: run_command(find_program('read_version.py')).stdout().strip(),
        license: 'GPL3',
        meson_version: '>= 0.56',
)
cc = meson.get_compiler('c')

#################### Configuration #################################
prefix = get_option('prefix')
bindir = prefix / get_option('bindir')
pkgdatadir = prefix / get_option('datadir') / meson.project_name()
pkglibexecdir = prefix / get_option('libexecdir') / meson.project_name()
sysconfdir = prefix / get_option('sysconfdir')


config = configuration_data()

config.set_quoted('SYSCONFDIR', sysconfdir)
config.set_quoted('PKGLIBEXECDIR', pkglibexecdir)
config.set_quoted('PKGDATADIR', pkgdatadir)
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('PACKAGE_VERSION', meson.project_version())
config.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())

r = run_command('realpath', '-m', '--relative-to=' + bindir, pkgdatadir)
config.set('bindir_to_pkgdatadir', r.stdout().strip())

r = run_command('realpath', '-m', '--relative-to=' + bindir, pkglibexecdir)
config.set_quoted('BIN_TO_PKGLIBEXECDIR', r.stdout().strip())

if host_machine.system() == 'windows'
    config.set('API_EXPORTED', '__declspec(dllexport)')
    config.set('LOCAL_SYMBOL', '')
    config.set('API_EXPORTED_RELOCATABLE', '__declspec(dllexport)')
    config.set('DEPRECATED', '__attribute__ ((deprecated))')
    config.set('HOTSPOT', '__attribute__ ((hot))')
    config.set_quoted('EXEEXT', '.exe')
else
    config.set('API_EXPORTED', '__attribute__ ((visibility ("protected")))')
    config.set('LOCAL_SYMBOL', '__attribute__ ((visibility ("hidden")))')
    config.set('API_EXPORTED_RELOCATABLE', '__attribute__ ((visibility ("default")))')
    config.set('DEPRECATED', '__attribute__ ((deprecated))')
    config.set('HOTSPOT', '__attribute__ ((hot))')
    config.set_quoted('EXEEXT', '')
endif


if host_machine.system() == 'windows'
    add_project_arguments('-DWIN32_LEAN_AND_MEAN', language : 'c')
    dlltool = find_program('dlltool', required : true)
endif

# meson modules
python = import('python').find_installation('python3')
config.set('python_path', python.full_path())


#################### target builds #################################

# write config file
configure_file(output: 'config.h', configuration : config)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

cstrgen = generator(
    python,
    output : ['@PLAINNAME@.inc'],
    arguments : [
	meson.current_source_dir() / 'devtools/convert-to-cstr.py',
	'-o', '@OUTPUT@',
	'@INPUT@'
    ]
)


all_c_sources = []
all_sources = []
deployed_targets = []

python3_required_modules = []
py_test_paths = []

subdir('src/mmpack')
subdir('src/pyscripts')
subdir('src/mmpack_build')
subdir('src/repository')
subdir('data')

all_sources += all_c_sources

# meson's python.dependency() is not working yet
# use this code picked from gtk-doc as a workaround
foreach p : python3_required_modules
    # Source: https://docs.python.org/3/library/importlib.html#checking-if-a-module-can-be-imported
    script = 'import importlib.util; import sys; exit(1) if importlib.util.find_spec(\''+ p +'\') is None else exit(0)'
    if run_command(python, '-c', script).returncode() != 0
        error('Required Python3 module \'' + p + '\' not found')
    endif
endforeach


#################### tests #################################
tests_state = 'disabled'
libcheck = dependency('check', required : get_option('tests'))
if libcheck.found() and not get_option('tests').disabled()
    tests_state = 'enabled'
    subdir('tests')
endif


#################### Docs generation #################################
docs_state = 'disabled'
sphinxbuild = find_program('sphinx-build', required : get_option('docs'))
if sphinxbuild.found() and not get_option('docs').disabled()
    # TODO
    # meson does not yet know how to check for a python module presence
    # (introduced in 0.51.0)
    # change this when possible
    python3 = import('python').find_installation('python3', required : true)
    check_linuxdoc = run_command(python3, '-c', '"import linuxdoc"')

    if check_linuxdoc.returncode() != 0 and get_option('docs').enabled()
        error('python3 module "linuxdoc" is required to build documentation')
    elif check_linuxdoc.returncode() == 0
        docs_state = 'enabled'
    endif
endif

if docs_state == 'enabled'
    subdir('docs')
endif


#################### Style checks #################################
# uses all the other meson files.
# keep at the end
subdir('devtools')


#################### Summary #################################
# print the status of the auto-detect features
summary({
  'tests': tests_state,
  'long-tests': get_option('long-tests'),
  'doc build': docs_state,
}, section: 'Configuration')
