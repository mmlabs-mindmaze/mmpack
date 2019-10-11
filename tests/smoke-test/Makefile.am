eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh

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

hello_world_SOURCES = hello-world.c
head_libexec_world_SOURCES = head-libexec-world.c
libexec_world_SOURCES = libexec-world.c
