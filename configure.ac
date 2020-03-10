# @mindmaze_header@

# Setup autoconf
AC_INIT(mmpack,[0.2.0],[gabriel.ganne@mindmaze.com])
AC_CONFIG_SRCDIR([src/mmpack/mmpack-update.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config/config.h])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

# Setup automake
AM_INIT_AUTOMAKE([no-dist-gzip dist-xz color-tests parallel-tests foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
MM_CC_WARNFLAGS
MM_OUTPUT_DEF
AC_SET_HOSTSYSTEM

# Check for programs
AC_CHECK_PROGS(RST2MAN, [rst2man rst2man3], [none])
AS_IF([test $RST2MAN = none], [AC_MSG_ERROR([rst2man is required for doc generation])])

AC_PROG_CC_C99
AM_PROG_CC_C_O
LT_INIT([win32-dll disable-static])
LT_SYS_MODULE_EXT
AC_SUBST(LIBTOOL_DEPS)
AM_PATH_PYTHON([3])
AC_PROG_SED

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

# Test for sphinx documentation with linuxdoc
AC_ARG_ENABLE([sphinxdoc], AC_HELP_STRING([--enable-sphinxdoc],*
	[Build sphinx documention. @<:@default=no@:>@]),
	[], [enable_sphinxdoc=detect])
AM_PATH_PYTHON([3])
MM_PYTHON_MODULE([sphinx])
MM_PYTHON_MODULE([sphinx_rtd_theme])
MM_PYTHON_MODULE([linuxdoc])
MM_PYTHON_MODULE([astroid], fatal_error)
MM_PYTHON_MODULE([urllib3], fatal_error)
AS_IF([test "$os_system" = win32], MM_PYTHON_MODULE([pefile], fatal_error))
AS_IF([test "$os_system" = posix], MM_PYTHON_MODULE([elftools], fatal_error))

AC_CHECK_PROG(HAVE_SPHINXBUILD, [sphinx-build], [$HAVE_PYMOD_LINUXDOC], [no])
AS_CASE([$enable_sphinxdoc],
        [yes], [AS_IF([test "x$HAVE_SPHINXBUILD" != xyes],
                      [AC_MSG_ERROR(Cannot find sphinx or its plugin linuxdoc)])],
        [detect], [AS_IF([test "x$HAVE_SPHINXBUILD" = xyes],
                         [enable_sphinxdoc=yes], [enable_sphinxdoc=no])])
AM_CONDITIONAL(BUILD_SPHINXDOC, [test "x$enable_sphinxdoc" = xyes])

AC_CHECK_PROG(SCAN_BUILD_6, [scan-build-6.0], [yes], [no])
AM_CONDITIONAL(HAVE_SCAN_BUILD_6, [test "x$SCAN_BUILD_6" = xyes])

# Check for libraries
MM_CHECK_LIB([mm_thr_create], [mmlib], MMLIB, [], [AC_MSG_ERROR([mmlib library required])])
MM_CHECK_LIB([yaml_parser_initialize], [yaml], YAML, [], [AC_MSG_ERROR([yaml library required])])
MM_CHECK_LIB([curl_easy_init], [curl], CURL, [], [AC_MSG_ERROR([curl library required])])
PKG_CHECK_MODULES_EXT(LIBARCHIVE, [libarchive], [], [AC_MSG_ERROR([libarchive library required])])

AC_DEF_API_EXPORT_ATTRS

AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /*CONFIG_H*/])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

