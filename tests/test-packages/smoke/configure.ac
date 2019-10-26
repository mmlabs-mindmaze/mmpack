# @mindmaze_header@

# Setup Automake
AC_INIT(hello, [1.0.0],[testMail@mail.com])
AC_CONFIG_SRCDIR([hello-world.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])


AC_CANONICAL_HOST
AC_CANONICAL_BUILD

# Check for programs
AC_PROG_CC_C99
AM_PROG_CC_C_O

LT_INIT([win32-dll])
LT_SYS_MODULE_EXT
AC_SUBST(LIBTOOL_DEPS)

AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

