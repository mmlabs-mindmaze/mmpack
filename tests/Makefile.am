eol=

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(eol)

AM_CFLAGS = \
	$(CHECK_CFLAGS) \
	$(MM_WARNFLAGS) \
	$(eol)

TESTS = unittests
check_PROGRAMS = \
	unittests \
	$(eol)

mmpack_sources = \
	../src/mmpack-common.c \
	../src/mmpack-common.h \
	../src/mmpack-config.c \
	../src/mmpack-config.h \
	../src/mmpack-update.c \
	../src/mmpack-update.h \
	$(eol)

unittests_SOURCES = \
	$(mmpack_sources) \
	config_test.c \
	testcases.h \
	unittests.c \
	$(eol)

unittests_LDADD = \
	$(CHECK_LIBS) \
	$(CURL_LIB) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)
