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

# Check for libraries
MM_CHECK_LIB([mmthr_create], [mmlib], MMLIB, [], [AC_MSG_ERROR([mmlib library required])])
MM_CHECK_LIB([yaml_parser_initialize], [libyaml], [], [AC_MSG_ERROR([yaml library required])])

AC_DEF_API_EXPORT_ATTRS
AC_SET_HOSTSYSTEM

AH_TOP([#ifndef CONFIG_H
#define CONFIG_H])
AH_BOTTOM([#endif /*CONFIG_H*/])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

