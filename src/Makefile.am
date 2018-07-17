eol=

AM_CFLAGS = $(MM_WARNFLAGS)

bin_PROGRAMS = mmpack

mmpack_SOURCES = \
	mmpack.c \
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
