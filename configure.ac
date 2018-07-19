# @mindmaze_header@

# Setup autoconf
AC_INIT(mmpack,[0.0],[gabriel.ganne@mindmaze.ch])
AC_CONFIG_SRCDIR([src/mmpack-update.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config/config.h])

# Setup automake
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz color-tests subdir-objects parallel-tests foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
MM_CC_WARNFLAGS
MM_OUTPUT_DEF

# Check for programs
AC_PROG_CC_C99
AM_PROG_CC_C_O
LT_INIT([win32-dll disable-static])
LT_SYS_MODULE_EXT
AC_SUBST(LIBTOOL_DEPS)
AM_PATH_PYTHON([3])

# Optional build of 'check' based tests
PKG_CHECK_MODULES_EXT(CHECK, [check >= 0.9.12],
                      [has_check=yes; tap_in_check=yes],
                      [PKG_CHECK_MODULES_EXT(CHECK, [check], [has_check=yes], [has_check=no])])
AC_ARG_WITH([check], AC_HELP_STRING([--with-check], [Build and run check based tests. @<:@default=detect@:>@]),
        [], [with_check=detect])
AS_CASE([$with_check],
		[yes], [AS_IF([test "$has_check" != no],
			[build_check_tests=yes],
			[AC_MSG_FAILURE([check required but not found])])
		],
		[no], [build_check_tests=no],
		[detect], [build_check_tests=$has_check],
		[AC_MSG_FAILURE([invalid argument for '--with-check' option])])
AM_CONDITIONAL(BUILD_CHECK_TESTS, [test "$build_check_tests" = yes])
AM_CONDITIONAL(TAP_SUPPORT_IN_CHECK, [test "x$tap_in_check" = xyes])

# Check for libraries
MM_CHECK_LIB([mmthr_create], [mmlib], MMLIB, [], [AC_MSG_ERROR([mmlib library required])])
MM_CHECK_LIB([yaml_parser_initialize], [yaml], YAML, [], [AC_MSG_ERROR([yaml library required])])
MM_CHECK_LIB([curl_easy_init], [curl], CURL, [], [AC_MSG_ERROR([curl library required])])

AC_DEF_API_EXPORT_ATTRS
AC_SET_HOSTSYSTEM

AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /*CONFIG_H*/])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT

