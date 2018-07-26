eol=

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-DSRCDIR=\"$(srcdir)\" \
	-DBUILDDIR=\"$(builddir)\" \
	$(CHECK_CPPFLAGS) \
	$(eol)

AM_CFLAGS = \
	$(CHECK_CFLAGS) \
	$(MM_WARNFLAGS) \
	$(eol)

clean-local:
	@rm -rvf venv

noinst_PYTHON = pytests.py

TESTS = unittests.tap python-unittests.tap
check_SCRIPTS = python-unittests.tap
check_PROGRAMS = \
	unittests.tap \
	$(eol)

TEST_EXTENSIONS = .test .tap
TAP_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
                 $(top_srcdir)/build-aux/tap-driver.sh

EXTRA_DIST = \
	create-mpkgs \
	run-test-mpkrepo \
	mmpack-config.yaml \
	test_version.py \
	$(eol)

mmpack_sources = \
	../src/sha256.c \
	../src/hash-utils.c \
	../src/mmpack-common.c \
	../src/mmpack-config.c \
	../src/mmpack-update.c \
	../src/package-utils.c \
	$(eol)

unittests_tap_SOURCES = \
	$(mmpack_sources) \
	config_test.c \
	hash_test.c \
	testcases.h \
	unittests.c \
	version_test.c \
	$(eol)

unittests_tap_LDADD = \
	$(CHECK_LIBS) \
	$(CURL_LIB) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)
