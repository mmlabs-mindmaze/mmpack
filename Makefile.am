eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh

AM_CPPFLAGS = \
	-I$(srcdir)/src \
	-DSRCDIR=\"$(srcdir)\" \
	-DBUILDDIR=\"$(builddir)\" \
	$(CHECK_CPPFLAGS) \
	$(LIBARCHIVE_CPPFLAGS) \
	$(eol)

AM_CFLAGS = \
	$(CHECK_CFLAGS) \
	$(LIBARCHIVE_CFLAGS) \
	$(MM_WARNFLAGS) \
	$(eol)

AM_V_RM = $(am__v_RM_$(V))
am__v_RM_ = $(am__v_RM_$(AM_DEFAULT_VERBOSITY))
am__v_RM_0 = -rm -f
am__v_RM_1 = -@rm -vf

bin_PROGRAMS = mmpack
pkglibexec_PROGRAMS = mount-mmpack-prefix
bin_SCRIPTS = mmpack-build
dist_bin_SCRIPTS = src/mmpack-createrepo
dist_pkgdata_SCRIPTS = src/check-dpkg-installed

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += \
	src/mmpack-build.in \
	src/settings.py.in \
	$(eol)

pkgpython_PYTHON =\
	src/binary_package.py \
	src/common.py \
	src/dpkg.py \
	src/decorators.py \
	src/elf_utils.py \
	src/file_utils.py \
	src/mmpack_config.py \
	src/mmpack_pkg_create.py \
	src/mmpack_pkg_check.py \
	src/mmpack_pkg_show.py \
	src/src_package.py \
	src/settings.py \
	src/version.py \
	src/workspace.py \
	$(eol)

.PHONY: spelling
spelling: $(pkgpython_PYTHON) $(mmpack_lib_sources)
	$(AM_V_AT) codespell $^

.PHONY: syntax
syntax: $(pkgpython_PYTHON)
	$(srcdir)/tools/check-pyfile $^


mmpack_lib_sources = \
	src/action-solver.c \
	src/action-solver.h \
	src/common.h \
	src/context.c \
	src/context.h \
	src/download.c \
	src/download.h \
	src/indextable.c \
	src/indextable.h \
	src/mm-alloc.h \
	src/mmpack-install.c \
	src/mmpack-install.h \
	src/mmpack-mkprefix.c \
	src/mmpack-mkprefix.h \
	src/mmpack-remove.c \
	src/mmpack-remove.h \
	src/mmpack-search.c \
	src/mmpack-search.h \
	src/mmpack-update.c \
	src/mmpack-update.h \
	src/mmstring.h \
	src/package-utils.c \
	src/package-utils.h \
	src/pkg-fs-utils.c \
	src/pkg-fs-utils.h \
	src/settings.c \
	src/settings.h \
	src/sha256.c \
	src/sha256.h \
	src/utils.c \
	src/utils.h \
	$(eol)

mmpack_SOURCES = \
	$(mmpack_lib_sources) \
	src/mmpack.c \
	$(eol)
pkgdata_DATA = \
	src/build-autotools \
	src/build-cmake \
	src/build-makefile \
	src/build-python \
	$(eol)

mmpack_LDADD = \
	$(CURL_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(eol)

mount_mmpack_prefix_SOURCES = \
	src/mount-prefix-linux.c \
	$(eol)

mmpack-build: src/mmpack-build.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<pythondir>,$(pythondir),g' < $< > $@

src/settings.py: src/settings.py.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<pkgdatadir>,$(pkgdatadir),g' < $< > $@

#
# TESTS RULES
#

clean-local:
	$(AM_V_RM) -r venv

noinst_PYTHON = tests/pytests.py

TESTS = unittests.tap tests/python-unittests.tap
dist_check_SCRIPTS = tests/python-unittests.tap
check_PROGRAMS = \
	unittests.tap \
	$(eol)

TEST_EXTENSIONS = .test .tap
TAP_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(srcdir)/build-aux/tap-driver.sh

EXTRA_DIST += \
	tests/create-mpks \
	tests/run-test-mpkrepo \
	tests/mmpack-config.yaml \
	tests/test_version.py \
	tests/binary-indexes \
	$(eol)

unittests_tap_SOURCES = \
	$(mmpack_lib_sources) \
	tests/binindex_test.c \
	tests/config_test.c \
	tests/dependencies_test.c \
	tests/indextable_test.c \
	tests/misc_test.c \
	tests/sha_test.c \
	tests/testcases.h \
	tests/unittests.c \
	tests/version_test.c \
	$(eol)

unittests_tap_LDADD = \
	$(CHECK_LIBS) \
	$(CURL_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)

completiondir = $(datadir)/bash-completion/completions
completion_DATA = \
	src/mmpack-build_completion \
	$(eol)
