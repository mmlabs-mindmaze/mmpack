if host_machine.system() == 'windows'
    add_project_arguments('-DEXEEXT=".exe"', language : 'c')
    exeext = '.exe'
else
    add_project_arguments('-DEXEEXT=""', language : 'c')
    exeext = ''
endif

# FIXME: autotools compat leftover
config.set('LT_MODULE_EXT', '"la"')

config.set('LOCALEDIR', get_option('localedir'))
config.set('SYSCONFDIR', get_option('sysconfdir'))
config.set('PACKAGE_NAME', '"' + meson.project_name() + '"')
config.set('PACKAGE_VERSION', '"' + meson.project_version() + '"')
config.set('PACKAGE_STRING', '"' + meson.project_name() + ' ' + meson.project_version() + '"')
config.set('LIBEXECDIR', '"' + get_option('libexecdir') + '"')
config.set('PKGDATADIR', '"' + get_option('datadir') + '"')
