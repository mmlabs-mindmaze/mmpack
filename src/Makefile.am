eol=

AM_CFLAGS = $(MM_WARNFLAGS)

bin_PROGRAMS = mmpack
dist_bin_SCRIPTS = mmpack-build

mmpack_builddir = $(pkgpythondir)
mmpack_build_PYTHON =\
	binary_package.py \
	common.py \
	decorators.py \
	dependencies.py \
	elf_utils.py \
	mmpack_config.py \
	mmpack_pkg_create.py \
	mmpack_build.py \
	mmpack_pkg_check.py \
	mmpack_pkg_show.py \
	package.py \
	settings.py \
	version.py \
	workspace.py \
	$(eol)


PYLINT_CONFIG = $(top_srcdir)/tools/pylint-config

.PHONY: syntax
syntax:
	$(foreach file, $(mmpack_build_PYTHON), \
			pycodestyle $(top_srcdir)/src/$(file) || exit;)
	$(foreach file, $(mmpack_build_PYTHON), \
			pylint3 --rcfile=$(PYLINT_CONFIG) $(top_srcdir)/src/$(file) || exit;)

check: syntax

mmpack_SOURCES = \
	hash-utils.c \
	hash-utils.h \
	mmpack.c \
	mmpack-common.c \
	mmpack-common.h \
	mmpack-config.c \
	mmpack-config.h \
	mmpack-update.c \
	mmpack-update.h \
	$(eol)

mmpack_LDADD = \
	$(CURL_LIB) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(CRYPTO_LIB) \
	$(eol)
