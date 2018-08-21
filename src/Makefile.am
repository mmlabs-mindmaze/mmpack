eol=

AM_CFLAGS = $(MM_WARNFLAGS)

bin_PROGRAMS = mmpack
bin_SCRIPTS = mmpack-build

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST = mmpack-build.in

pkgpython_PYTHON =\
	binary_package.py \
	common.py \
	decorators.py \
	dependencies.py \
	elf_utils.py \
	mmpack_config.py \
	mmpack_pkg_create.py \
	mmpack_pkg_check.py \
	mmpack_pkg_show.py \
	package.py \
	settings.py \
	version.py \
	workspace.py \
	$(eol)


.PHONY: syntax
syntax: $(pkgpython_PYTHON)
	$(top_srcdir)/tools/check-pyfile $^


mmpack_SOURCES = \
	binary-index.c \
	binary-index.h \
	indextable.c \
	indextable.h \
	mm-alloc.h \
	mmpack.c \
	mmpack-common.c \
	mmpack-common.h \
	mmpack-context.c \
	mmpack-context.h \
	mmpack-config.c \
	mmpack-config.h \
	mmpack-update.c \
	mmpack-update.h \
	mmstring.h \
	package-utils.c \
	package-utils.h \
	sha256.c \
	sha256.h \
	$(eol)

mmpack_LDADD = \
	$(CURL_LIB) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)

mmpack-build: mmpack-build.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<pythondir>,$(pythondir),g' < $< > $@
