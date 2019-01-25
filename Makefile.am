eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh

AM_CPPFLAGS = \
	-I$(srcdir)/src \
	-DSRCDIR=\"$(srcdir)\" \
	-DBUILDDIR=\"$(builddir)\" \
	-DLIBEXECDIR=\"$(pkglibexecdir)\" \
	-DEXEEXT=\"$(EXEEXT)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	$(CHECK_CPPFLAGS) \
	$(LIBARCHIVE_CPPFLAGS) \
	$(eol)

if OS_TYPE_WIN32
AM_CPPFLAGS += -DWIN32_LEAN_AND_MEAN
endif

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
pkglibexec_PROGRAMS = mount-mmpack-prefix mmpack-check-sysdep
bin_SCRIPTS = mmpack-build
dist_bin_SCRIPTS = src/mmpack-createrepo
dist_pkgdata_SCRIPTS = src/check-dpkg-installed

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += \
	src/mmpack-build.in \
	src/settings.py.in \
	$(eol)

mmpackbuilddir = $(pkgdatadir)/python
mmpackbuild_PYTHON =\
	src/binary_package.py \
	src/common.py \
	src/dpkg.py \
	src/decorators.py \
	src/elf_utils.py \
	src/file_utils.py \
	src/mmpack_builddep.py \
	src/mmpack_clean.py \
	src/mmpack_config.py \
	src/mmpack_pkg_create.py \
	src/mmpack_pkg_check.py \
	src/mmpack_pkg_show.py \
	src/pacman.py \
	src/pe_utils.py \
	src/src_package.py \
	src/mm_version.py \
	src/workspace.py \
	src/xdg.py \
	$(eol)

nodist_mmpackbuild_PYTHON =\
	src/settings.py \
	$(eol)

BUILT_SOURCES = $(nodist_mmpackbuild_PYTHON)
CLEANFILES += $(nodist_mmpackbuild_PYTHON)


.PHONY: spelling
spelling: $(mmpackbuild_PYTHON) $(mmpack_lib_sources) $(RST_MAN_PAGES_IN)
	$(AM_V_at) codespell $^

.PHONY: syntax
syntax: $(mmpackbuild_PYTHON) $(nodist_mmpackbuild_PYTHON)
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
	src/mmpack-check-integrity.c \
	src/mmpack-check-integrity.h \
	src/mmpack-install.c \
	src/mmpack-install.h \
	src/mmpack-list.c \
	src/mmpack-list.h \
	src/mmpack-mkprefix.c \
	src/mmpack-mkprefix.h \
	src/mmpack-remove.c \
	src/mmpack-remove.h \
	src/mmpack-run.c \
	src/mmpack-run.h \
	src/mmpack-search.c \
	src/mmpack-search.h \
	src/mmpack-show.c \
	src/mmpack-show.h \
	src/mmpack-source.c \
	src/mmpack-source.h \
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
	src/sysdeps.c \
	src/sysdeps.h \
	src/utils.c \
	src/utils.h \
	$(eol)

mmpack_SOURCES = \
	$(mmpack_lib_sources) \
	src/mmpack.c \
	$(eol)
dist_pkgdata_DATA = \
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
	$(eol)


mount_mmpack_prefix_SOURCES =

if OS_KERNEL_LINUX
mount_mmpack_prefix_SOURCES += src/mount-prefix-linux.c
endif

if OS_KERNEL_NT
mount_mmpack_prefix_SOURCES += src/mount-prefix-nt.c
endif

mmpack_check_sysdep_SOURCES = \
	src/indextable.h \
	src/indextable.c \
	src/mmpack-check-sysdep.c \
	src/sha256.c \
	src/sysdeps.h \
	src/sysdeps.c \
	src/utils.c \
	$(eol)

mmpack_check_sysdep_LDADD = \
	$(MMLIB_LIB) \
	$(eol)


# dev debian-only helper function, not part of any target
# requires root permissions to run
.PHONY: setcap
setcap:
	$(AM_V_GEN) setcap cap_dac_override,cap_sys_admin+ep $(libexecdir)/mmpack/mount-mmpack-prefix

mmpack-build: src/mmpack-build.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<mmpackbuilddir>,$(mmpackbuilddir),g' < $< > $@
	$(AM_V_at)chmod a+x $@

%.py : %.py.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<pkgdatadir>,$(pkgdatadir),g' \
		-e 's,<libexecdir>,$(libexecdir),g' < $< > $@

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
dist_completion_DATA = \
	src/mmpack_completion \
	src/mmpack-build_completion \
	$(eol)

#
# DOC RULES
#

RST_MAN_PAGES_IN = \
	docs/mmpack.rst.in \
	docs/mmpack-build.rst.in \
	docs/mmpack-build-builddep.rst.in \
	docs/mmpack-build-clean.rst.in \
	docs/mmpack-build-pkg-create.rst.in \
	docs/mmpack-check-integrity.rst.in \
	docs/mmpack-install.rst.in \
	docs/mmpack-list.rst.in \
	docs/mmpack-mkprefix.rst.in \
	docs/mmpack-remove.rst.in \
	docs/mmpack-run.rst.in \
	docs/mmpack-search.rst.in \
	docs/mmpack-show.rst.in \
	docs/mmpack-source.rst.in \
	docs/mmpack-update.rst.in \
	$(eof)
EXTRA_DIST += $(RST_MAN_PAGES_IN)

nodist_man_MANS = $(RST_MAN_PAGES_IN:.rst.in=.1)
CLEANFILES += $(nodist_man_MANS)

%.1: %.rst.in
	$(AM_V_at) $(MKDIR_P) docs
	$(AM_V_GEN) $(SED) -e 's,<version>,$(VERSION),g' < $< | $(RST2MAN) > $@
