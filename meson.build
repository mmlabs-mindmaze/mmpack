project('mmpack', 'c',)

cc = meson.get_compiler('c')
config = configuration_data()

subdir('config/api-exports')
subdir('config/autotools-compat')

if host_machine.system() == 'windows'
    add_project_arguments('-DWIN32_LEAN_AND_MEAN', language : 'c')
endif

# meson modules
python = import('python').find_installation('python3', required : true)

# write config file
build_cfg = 'config.h'  # named as such to match autotools build system
configure_file(output : build_cfg, configuration : config)

# define HAVE_CONFIG_H with compiler command line to include the generated
# config.h file (same as autotools)
add_project_arguments('-DHAVE_CONFIG_H', language : 'c')

libcurl = dependency('libcurl', required : true)
libarchive = dependency('libarchive', required : true)
libyaml = cc.find_library('libyaml', required : true)
libmmlib = cc.find_library('mmlib', required : true)

subdir('src/mmpack')
subdir('src/mmpack-build')
subdir('src/repository')

deployment_dir = meson.build_root() + '/deployment-test'
install_manifest = custom_target('install-manifest',
        output : 'install-manifest',
        capture : true,
        command : [ 'meson', 'introspect', '--install' ],
        build_by_default : true,
)

do_deployment_test = custom_target('deployment-test',
        output : 'deployment-test',
        command : [ 'python3',
                    files('tests/do-deployment-test.py'),
                    deployment_dir,
                    install_manifest,
        ],
        depends : [install_manifest, mmpack, mount_mmpack_prefix,
                   mmpack_check_sysdep ],
        depend_files : [ mmpack_build_sources ],
        build_by_default : true,
)

run_target('deployment-test-clean',
    command : [ 'rm', '-rvf', deployment_dir ],
)

if get_option('tests')
    subdir('tests')
endif

if get_option('doc')
    subdir('docs')
endif

# uses all the other meson files.
# keep at the end
subdir('devtools')
