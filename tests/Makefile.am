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


AM_V_RM = $(am__v_RM_$(V))
am__v_RM_ = $(am__v_RM_$(AM_DEFAULT_VERBOSITY))
am__v_RM_0 = -rm -f
am__v_RM_1 = -@rm -vf

clean-local:
	$(AM_V_RM) -r venv

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
	../src/indextable.c \
	../src/mmpack-common.c \
	../src/mmpack-context.c \
	../src/mmpack-config.c \
	../src/mmpack-update.c \
	../src/package-utils.c \
	../src/sha256.c \
	$(eol)

unittests_tap_SOURCES = \
	$(mmpack_sources) \
	config_test.c \
	indextable_test.c \
	sha_test.c \
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
