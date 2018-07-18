eol=

AM_CFLAGS = $(MM_WARNFLAGS)

bin_PROGRAMS = mmpack

PYTHON_INTERNALS = \
	common.py \
	dependencies.py \
	elf_utils.py \
	package.py \
	settings.py \
	singleton_decorator.py \
	version.py \
	workspace.py \
	$(eol)

.PHONY: syntax
syntax:
	$(foreach file, $(PYTHON_INTERNALS), pycodestyle $(top_srcdir)/src/$(file);)
	$(foreach file, $(PYTHON_INTERNALS), pylint3 -rn $(top_srcdir)/src/$(file);)

check: syntax

mmpack_SOURCES = \
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
	$(eol)
