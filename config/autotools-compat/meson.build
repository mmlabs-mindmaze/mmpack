if host_machine.system() == 'windows'
    exeext = '.exe'
else
    exeext = ''
endif
config.set_quoted('EXEEXT', exeext)

prefix = get_option('prefix')
config.set_quoted('LOCALEDIR', prefix / get_option('localedir'))
config.set_quoted('SYSCONFDIR', prefix / get_option('sysconfdir'))
config.set_quoted('LIBEXECDIR', prefix / get_option('libexecdir'))
config.set_quoted('PKGDATADIR', prefix / get_option('datadir'))
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('PACKAGE_VERSION', meson.project_version())
config.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
