eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh
lib_LTLIBRARIES = libhello.la

AM_CPPFLAGS = \
	-DLIBEXECDIR=\"$(pkglibexecdir)\" \
	-DEXEEXT=\"$(EXEEXT)\" \
	$(eol)

AM_CFLAGS = -Wall -Wextra -pedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Werror

bin_PROGRAMS = \
	hello-world \
	head-libexec-world \
	$(eol)

pkglibexec_PROGRAMS = \
	libexec-world \
	$(eol)

bin_SCRIPTS = \
	shell-exec.py

include_HEADERS =libhello.h

libhello_la_SOURCES = \
	libhello.c \
	libhello.h

libhello_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-no-undefined \
	-version-info 1:0:0

hiddendir = $(pkgdatadir)/.hidden
hidden_DATA = afile

hello_world_SOURCES = hello-world.c
hello_world_LDADD = libhello.la

head_libexec_world_SOURCES = head-libexec-world.c
libexec_world_SOURCES = libexec-world.c
