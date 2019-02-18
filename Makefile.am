eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh

AM_CPPFLAGS = \
	-I$(srcdir)/src/mmpack \
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
dist_bin_SCRIPTS = src/repository/mmpack-createrepo
dist_pkgdata_SCRIPTS = src/mmpack/check-dpkg-installed

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += \
	src/mmpack-build/mmpack-build.in \
	src/mmpack-build/settings.py.in \
	$(eol)

mmpackbuilddir = $(pkgdatadir)/python
mmpackbuild_PYTHON =\
	src/mmpack-build/binary_package.py \
	src/mmpack-build/common.py \
	src/mmpack-build/dpkg.py \
	src/mmpack-build/decorators.py \
	src/mmpack-build/elf_utils.py \
	src/mmpack-build/file_utils.py \
	src/mmpack-build/mmpack_builddep.py \
	src/mmpack-build/mmpack_clean.py \
	src/mmpack-build/mmpack_pkg_create.py \
	src/mmpack-build/pacman.py \
	src/mmpack-build/pe_utils.py \
	src/mmpack-build/src_package.py \
	src/mmpack-build/mm_version.py \
	src/mmpack-build/workspace.py \
	src/mmpack-build/xdg.py \
	$(eol)

nodist_mmpackbuild_PYTHON =\
	src/mmpack-build/settings.py \
	$(eol)

BUILT_SOURCES = $(nodist_mmpackbuild_PYTHON)
CLEANFILES += $(nodist_mmpackbuild_PYTHON)


.PHONY: spelling
spelling: $(mmpackbuild_PYTHON) $(mmpack_lib_sources) $(RST_MAN_PAGES_IN)
	$(AM_V_at) codespell $^

.PHONY: syntax
syntax: $(mmpackbuild_PYTHON) $(nodist_mmpackbuild_PYTHON)
	$(srcdir)/devtools/check-pyfile $^


mmpack_lib_sources = \
	src/mmpack/action-solver.c \
	src/mmpack/action-solver.h \
	src/mmpack/common.h \
	src/mmpack/context.c \
	src/mmpack/context.h \
	src/mmpack/download.c \
	src/mmpack/download.h \
	src/mmpack/indextable.c \
	src/mmpack/indextable.h \
	src/mmpack/mm-alloc.h \
	src/mmpack/mmpack-check-integrity.c \
	src/mmpack/mmpack-check-integrity.h \
	src/mmpack/mmpack-fix-broken.c \
	src/mmpack/mmpack-fix-broken.h \
	src/mmpack/mmpack-install.c \
	src/mmpack/mmpack-install.h \
	src/mmpack/mmpack-list.c \
	src/mmpack/mmpack-list.h \
	src/mmpack/mmpack-mkprefix.c \
	src/mmpack/mmpack-mkprefix.h \
	src/mmpack/mmpack-remove.c \
	src/mmpack/mmpack-remove.h \
	src/mmpack/mmpack-run.c \
	src/mmpack/mmpack-run.h \
	src/mmpack/mmpack-search.c \
	src/mmpack/mmpack-search.h \
	src/mmpack/mmpack-show.c \
	src/mmpack/mmpack-show.h \
	src/mmpack/mmpack-source.c \
	src/mmpack/mmpack-source.h \
	src/mmpack/mmpack-update.c \
	src/mmpack/mmpack-update.h \
	src/mmpack/mmpack-upgrade.c \
	src/mmpack/mmpack-upgrade.h \
	src/mmpack/mmstring.h \
	src/mmpack/package-utils.c \
	src/mmpack/package-utils.h \
	src/mmpack/pkg-fs-utils.c \
	src/mmpack/pkg-fs-utils.h \
	src/mmpack/settings.c \
	src/mmpack/settings.h \
	src/mmpack/sha256.c \
	src/mmpack/sha256.h \
	src/mmpack/sysdeps.c \
	src/mmpack/sysdeps.h \
	src/mmpack/utils.c \
	src/mmpack/utils.h \
	$(eol)

mmpack_SOURCES = \
	$(mmpack_lib_sources) \
	src/mmpack/mmpack.c \
	$(eol)
dist_pkgdata_DATA = \
	src/mmpack-build/build-autotools \
	src/mmpack-build/build-cmake \
	src/mmpack-build/build-makefile \
	src/mmpack-build/build-python \
	$(eol)

mmpack_LDADD = \
	$(CURL_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)


mount_mmpack_prefix_SOURCES =

if OS_KERNEL_LINUX
mount_mmpack_prefix_SOURCES += src/mmpack/mount-prefix-linux.c
endif

if OS_KERNEL_NT
mount_mmpack_prefix_SOURCES += src/mmpack/mount-prefix-nt.c
endif

mmpack_check_sysdep_SOURCES = \
	src/mmpack/indextable.h \
	src/mmpack/indextable.c \
	src/mmpack/mmpack-check-sysdep.c \
	src/mmpack/sha256.c \
	src/mmpack/sysdeps.h \
	src/mmpack/sysdeps.c \
	src/mmpack/utils.c \
	$(eol)

mmpack_check_sysdep_LDADD = \
	$(MMLIB_LIB) \
	$(eol)


# dev debian-only helper function, not part of any target
# requires root permissions to run
.PHONY: setcap
setcap:
	$(AM_V_GEN) setcap cap_dac_override,cap_sys_admin+ep $(libexecdir)/mmpack/mount-mmpack-prefix

mmpack-build: src/mmpack-build/mmpack-build.in Makefile
	$(AM_V_GEN)$(SED) -e 's,<mmpackbuilddir>,$(mmpackbuilddir),g' < $< > $@
	$(AM_V_at)chmod a+x $@

%.py : %.py.in Makefile
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(SED) -e 's,<pkgdatadir>,$(pkgdatadir),g' \
		-e 's,<libexecdir>,$(libexecdir),g' < $< > $@

#
# TESTS RULES
#

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
	src/mmpack/mmpack_completion \
	src/mmpack-build/mmpack-build_completion \
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
	docs/mmpack-fix-broken.rst.in \
	docs/mmpack-install.rst.in \
	docs/mmpack-list.rst.in \
	docs/mmpack-mkprefix.rst.in \
	docs/mmpack-remove.rst.in \
	docs/mmpack-run.rst.in \
	docs/mmpack-search.rst.in \
	docs/mmpack-show.rst.in \
	docs/mmpack-source.rst.in \
	docs/mmpack-update.rst.in \
	docs/mmpack-upgrade.rst.in \
	$(eof)
EXTRA_DIST += $(RST_MAN_PAGES_IN)

nodist_man_MANS = $(RST_MAN_PAGES_IN:.rst.in=.1)
CLEANFILES += $(nodist_man_MANS)

%.1: %.rst.in
	$(AM_V_at) $(MKDIR_P) docs
	$(AM_V_GEN) $(SED) -e 's,<version>,$(VERSION),g' < $< | $(RST2MAN) > $@

#
# optional doc rules: generate internal function documentation
# Skipped if --enable-sphinxdoc=no
#
if BUILD_SPHINXDOC
SPHINXBUILD   = sphinx-build

sphinx_verbose = $(sphinx_verbose_@AM_V@)
sphinx_verbose_ = $(sphinx_verbose_@AM_DEFAULT_V@)
sphinx_verbose_0 = -q

ALLSPHINXOPTS = -d $(sphinx_verbose) $(srcdir)

# Due to a dependency bug in sphinx-build, we use the -M option instead of
# the usual -b one. This hidden option sums up to the same as -b with makefile
# dependency support ... except that it does not support -W option which turns
# warnings into errors !
# => do this with a simple grep line on the output

index.rst: $(srcdir)/docs/gen-devdoc-mmpack.sh $(filter %.c, $(mmpack_lib_sources))
	$(AM_V_GEN) $^ > $@

conf.py: $(srcdir)/docs/conf.py
	$(AM_V_GEN) cp -uv $^ $@

sphinx-build.log: conf.py index.rst
	@$(RM) -f html-buildstamp
	$(AM_V_GEN) SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir) srctree=$(srcdir) $(SPHINXBUILD) -b html -c $(srcdir)/docs/ $(builddir) $(builddir)/html &> $@

# eg. "src/mmpack-build/common.py" -> "common.rst"
mmpackbuild_doc_tmp = $(notdir $(mmpackbuild_PYTHON))
mmpackbuild_docfile = $(mmpackbuild_doc_tmp:.py=.rst)
$(mmpackbuild_docfile): modules.rst
modules.rst: $(mmpackbuild_PYTHON) $(nodist_mmpackbuild_PYTHON)
	$(AM_V_GEN) SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir) sphinx-apidoc -o . $(srcdir)/src/mmpack-build/

html-local: html-buildstamp
html-buildstamp: sphinx-build.log $(mmpackbuild_docfile)
	@rm -vf $@
	@if [ "$(shell grep -c WARNING: sphinx-build.log)" != "0" ] ; then \
		grep WARNING: sphinx-build.log; \
		false; \
	fi
	@echo
	@echo "Build finished. The HTML pages are in $(builddir)/html."
	@touch $@

html-local: html-buildstamp

html-clean:
	$(RM) -rf html html-buildstamp sphinx-build.log conf.py index.rst $(mmpackbuild_docfile) modules.rst

clean-local: html-clean

endif # BUILD_SPHINXDOC
