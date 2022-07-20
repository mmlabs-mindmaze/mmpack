project('mmpack', 'c',
        version: run_command(find_program('read_version.py')).stdout().strip(),
        license: 'GPL3',
        meson_version: '>= 0.50',
)
cc = meson.get_compiler('c')
config = configuration_data()

subdir('config/api-exports')
subdir('config/autotools-compat')

if host_machine.system() == 'windows'
    add_project_arguments('-DWIN32_LEAN_AND_MEAN', language : 'c')

    # Store in config the relative path from installed bindir to installed libexecdir
    bindir = get_option('prefix') / get_option('bindir')
    libexecdir = get_option('prefix') / get_option('libexecdir')
    r = run_command('realpath', '-m', '--relative-to=' + bindir, libexecdir)
    config.set_quoted('BIN_TO_LIBEXECDIR', r.stdout().strip())
endif

find_program('zstd', required : true)
find_program('tar', required : true)

# meson modules
python = import('python').find_installation('python3', required : true)

python3_required_modules = ['astroid', 'urllib3']
if host_machine.system() == 'windows'
    python3_required_modules += ['pefile']
elif host_machine.system() == 'linux'
    python3_required_modules += ['elftools']
endif

# meson's python.dependency() is not working yet
# use this code picked from gtk-doc as a workaround
foreach p : python3_required_modules
    # Source: https://docs.python.org/3/library/importlib.html#checking-if-a-module-can-be-imported
    script = 'import importlib.util; import sys; exit(1) if importlib.util.find_spec(\''+ p +'\') is None else exit(0)'
    if run_command(python, '-c', script).returncode() != 0
        error('Required Python3 module \'' + p + '\' not found')
    endif
endforeach

# write config file
build_cfg = 'config.h'  # named as such to match autotools build system
configure_file(output : build_cfg, configuration : config)

# define HAVE_CONFIG_H with compiler command line to include the generated
# config.h file (same as autotools)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

libcurl = dependency('libcurl', required : true)
libarchive = dependency('libarchive', required : true)
zlib = dependency('zlib', required : true)
libyaml = cc.find_library('libyaml', required : true)
libmmlib = cc.find_library('mmlib', required : true)

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


subdir('common-licenses')
subdir('src/mmpack')
subdir('src/mmpack-build')
subdir('src/repository')
subdir('data')

deployment_dir = meson.build_root() + '/deployment-test'
install_manifest = custom_target('install-manifest',
        output : 'install-manifest',
        capture : true,
        command : [ 'meson', 'introspect', '--installed' ],
        build_by_default : true,
)

do_deployment_test = custom_target('deployment-test',
        output : 'deployment-test',
        command : [ 'python3',
                    files('tests/do-deployment-test.py'),
                    deployment_dir,
                    install_manifest,
        ],
        depends : [install_manifest, mmpack, mount_mmpack_prefix],
        depend_files : [ mmpack_build_sources ],
        build_by_default : true,
)

run_target('deployment-test-clean',
    command : [ 'rm', '-rvf', deployment_dir ],
)

repo_dir = meson.build_root() + '/test-repo'
do_test_repo = custom_target('test-repo',
        output: 'test-repo',
        command : ['bash',
                   files('tests/create-test-repo.sh'),
                   deployment_dir,
                   repo_dir,
                   get_option('prefix') / get_option('bindir'),
        ],
        depends : do_deployment_test,
)

sysrepo_dir = meson.build_root() + '/test-sysrepo'
do_test_sysrepo = custom_target('test-sysrepo',
        output: 'test-sysrepo',
        command : ['bash',
                   files('tests/create-test-sysrepo.sh'),
                   sysrepo_dir,
                   '@0@'.format(get_option('long-tests')),
        ],
)

testpkg_dir = meson.build_root() + '/test-packages'
do_test_pkg = custom_target('test-pkg',
        output: 'test-packages',
        command : ['bash',
                   files('tests/create-test-pkg.sh'),
                   testpkg_dir,
                   '@0@'.format(get_option('long-tests')),
        ],
)

tests_state = 'disabled'
libcheck = dependency('check', required : get_option('tests'))
if libcheck.found() and not get_option('tests').disabled()
    tests_state = 'enabled'
    subdir('tests')
endif

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

# uses all the other meson files.
# keep at the end
subdir('devtools')

# print the status of the auto-detect features
message('Configuration summary')
message('=====================')
message('* Tests : @0@'.format(tests_state))
message('  - run long tests : @0@'.format(get_option('long-tests')))
message('* Doc building : @0@'.format(docs_state))
