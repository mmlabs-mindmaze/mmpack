project('mmpack', 'c',)

cc = meson.get_compiler('c')
config = configuration_data()

subdir('config/api-exports')
subdir('config/autotools-compat')

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

if get_option('tests')
    subdir('tests')
endif

if get_option('doc')
    subdir('docs')
endif

# uses all the other meson files.
# keep at the end
subdir('devtools')
