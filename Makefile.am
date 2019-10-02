eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh TODO.md LICENSE

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
bin_SCRIPTS = src/mmpack-build/mmpack-build
dist_bin_SCRIPTS = \
	src/repository/mmpack-createrepo \
	devtools/mmpack-build-slave.sh \
	$(eol)
dist_pkgdata_SCRIPTS = src/mmpack/check-dpkg-installed

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += \
	src/mmpack-build/settings.py.in \
	$(eol)

mmpackbuilddir = $(pythondir)/mmpack_build
mmpackbuild_PYTHON =\
	src/mmpack-build/__init__.py \
	src/mmpack-build/__main__.py \
	src/mmpack-build/base_hook.py \
	src/mmpack-build/binary_package.py \
	src/mmpack-build/common.py \
	src/mmpack-build/dpkg.py \
	src/mmpack-build/decorators.py \
	src/mmpack-build/elf_utils.py \
	src/mmpack-build/file_utils.py \
	src/mmpack-build/hook_python.py \
	src/mmpack-build/hook_sharedlib.py \
	src/mmpack-build/hooks_loader.py \
	src/mmpack-build/mmpack_builddep.py \
	src/mmpack-build/mmpack_clean.py \
	src/mmpack-build/mmpack_pkg_create.py \
	src/mmpack-build/pacman.py \
	src/mmpack-build/pe_utils.py \
	src/mmpack-build/provide.py \
	src/mmpack-build/src_package.py \
	src/mmpack-build/mm_version.py \
	src/mmpack-build/workspace.py \
	src/mmpack-build/xdg.py \
	$(eol)

mmpack_build_gerritdir =  $(pkgdatadir)/gerrit/python
mmpack_build_gerrit_PYTHON = \
	devtools/gerrit/gerrit-mmpack-build.py \
	devtools/gerrit/gerrit.py \
	$(eol)

mmpack_db_service = \
	src/repository/mmpack-db.py \
	$(eol)

nodist_mmpackbuild_PYTHON =\
	src/mmpack-build/settings.py \
	$(eol)

BUILT_SOURCES = $(nodist_mmpackbuild_PYTHON)
CLEANFILES += $(nodist_mmpackbuild_PYTHON)


.PHONY: pycodestyle
pycodestyle: deployment-test
	pycodestyle --show-source deployment-test$(pythondir)/mmpack_build

.PHONY: pylint
pylint: devtools/pylint-config deployment-test
	pylint3 --jobs=0 --disable=locally-disabled --rcfile=$< deployment-test$(pythondir)/mmpack_build

.PHONY: syntax
syntax: pycodestyle pylint
	@echo "$@ target is obsolete, and will be removed soon."
	@echo "Use \"pycodestyle\" and \"pylint\" instead."

mmpack_lib_sources = \
	src/mmpack/action-solver.c \
	src/mmpack/action-solver.h \
	src/mmpack/cmdline.c \
	src/mmpack/cmdline.h \
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
	src/mmpack/mmpack-download.c \
	src/mmpack/mmpack-download.h \
	src/mmpack/mmpack-fix-broken.c \
	src/mmpack/mmpack-fix-broken.h \
	src/mmpack/mmpack-install.c \
	src/mmpack/mmpack-install.h \
	src/mmpack/mmpack-list.c \
	src/mmpack/mmpack-list.h \
	src/mmpack/mmpack-mkprefix.c \
	src/mmpack/mmpack-mkprefix.h \
	src/mmpack/mmpack-provides.c \
	src/mmpack/mmpack-provides.h \
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

if OS_TYPE_WIN32

mmpack_lib_sources += \
	src/mmpack/path-win32.c \
	src/mmpack/path-win32.h \
	$(eol)

endif

mmpack_SOURCES = \
	$(mmpack_lib_sources) \
	src/mmpack/mmpack.c \
	$(eol)
dist_pkgdata_DATA = \
	src/mmpack/prompt.bash \
	src/mmpack-build/build-autotools \
	src/mmpack-build/build-cmake \
	src/mmpack-build/build-makefile \
	src/mmpack-build/build-meson \
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


# write the 2nd @ between [] in order to prevent autotools replacing it
# BEFORE the sed call
%.py : %.py.in Makefile
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(SED) -e 's,@pkgdatadir[@],$(pkgdatadir),g' \
		-e 's,@libexecdir[@],$(libexecdir),g' \
		-e 's,@bindir[@],$(bindir),g' \
		-e 's,@exeext[@],$(EXEEXT),g' < $< > $@

#
# DEVEL TARGETS
#
# Those should only make sense as helpers in a development context

user_doc_sources =
.PHONY: spelling
spelling: $(mmpackbuild_PYTHON) $(mmpack_lib_sources) $(RST_MAN_PAGES) \
		$(mmpack_build_gerrit_PYTHON) $(user_doc_sources) $(mmpack_db_service)
	$(AM_V_at) codespell $^

# dev debian-only helper function, not part of any target
# requires root permissions to run
.PHONY: setcap
setcap:
	$(AM_V_GEN) setcap cap_dac_override,cap_sys_admin+ep $(libexecdir)/mmpack/mount-mmpack-prefix

SCAN_BUILD_OPTIONS = \
	-enable-checker alpha.core.BoolAssignment \
	-enable-checker alpha.core.IdenticalExpr \
	-enable-checker alpha.core.SizeofPtr \
	$(eol)

# if scan-build 6.0 (from backport) is available, add some more checkers
if HAVE_SCAN_BUILD_6
SCAN_BUILD_OPTIONS += \
	-enable-checker alpha.core.PointerArithm \
	-enable-checker alpha.core.PointerSub \
	-enable-checker alpha.deadcode.UnreachableCode \
	-enable-checker alpha.security.MallocOverflow \
	-enable-checker nullability.NullableDereferenced \
	-enable-checker nullability.NullablePassedToNonnull \
	-enable-checker nullability.NullableReturnedFromNonnull \
	$(eol)
endif

.PHONY: scan-build
scan-build:
	scan-build -analyze-headers $(SCAN_BUILD_OPTIONS) $(MAKE)

EXTRA_DIST += devtools/uncrustify.cfg
.PHONY: fix-c-style
fix-c-style: devtools/uncrustify.cfg
	cd $(srcdir) && uncrustify -c devtools/uncrustify.cfg -l C --replace --no-backup \
		$(mmpack_SOURCES) $(mmpack_check_sysdep_SOURCES) \
		$(mount_mmpack_prefix_SOURCES)

# check all the source files for correct coding style
.PHONY: checkstyle
checkstyle: devtools/uncrustify.cfg pycodestyle
	cd $(srcdir) && uncrustify -c devtools/uncrustify.cfg -l C --check \
	   $(mmpack_SOURCES) $(mmpack_check_sysdep_SOURCES) \
	   $(mount_mmpack_prefix_SOURCES)

#
# TESTS RULES
#

deployment-test: $(mmpack_SOURCES) $(mmpack_check_sysdep_SOURCES) \
		$(mount_mmpack_prefix_SOURCES) $(mmpackbuild_PYTHON) \
		$(nodist_mmpackbuild_PYTHON)
	DESTDIR=$(shell readlink -f $@) $(MAKE) install
	@touch $@

deployment-test-clean:
	@$(RM) -rvf deployment-test

noinst_PYTHON = tests/pytests.py

check_DATA = deployment-test
TESTS = unittests.tap tests/python-unittests.tap
dist_check_SCRIPTS = tests/python-unittests.tap
check_PROGRAMS = \
	unittests.tap \
	$(eol)

TESTS_ENVIRONMENT = DEPLOYMENT_DIR=deployment-test PYTHON_INSTALL_DIR=$(pythondir) SRCDIR=$(srcdir)
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

RST_MAN_PAGES = \
	docs/man/mmpack.rst \
	docs/man/mmpack-build.rst \
	docs/man/mmpack-build-builddep.rst \
	docs/man/mmpack-build-clean.rst \
	docs/man/mmpack-build-pkg-create.rst \
	docs/man/mmpack-check-integrity.rst \
	docs/man/mmpack-download.rst \
	docs/man/mmpack-fix-broken.rst \
	docs/man/mmpack-install.rst \
	docs/man/mmpack-list.rst \
	docs/man/mmpack-mkprefix.rst \
	docs/man/mmpack-provides.rst \
	docs/man/mmpack-remove.rst \
	docs/man/mmpack-run.rst \
	docs/man/mmpack-search.rst \
	docs/man/mmpack-show.rst \
	docs/man/mmpack-source.rst \
	docs/man/mmpack-update.rst \
	docs/man/mmpack-upgrade.rst \
	$(eof)
EXTRA_DIST += $(RST_MAN_PAGES)

nodist_man_MANS = $(RST_MAN_PAGES:.rst=.1)
CLEANFILES += $(nodist_man_MANS)

# rst2man is part of the docutils package which is a dependency of sphinx
%.1: %.rst
	$(AM_V_at) $(MKDIR_P) docs/man
	$(AM_V_GEN) $(RST2MAN) $< $@

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

conf.py: $(srcdir)/docs/devdoc-conf.py
	$(AM_V_GEN) cp -uv $^ $@

sphinx-build.log: conf.py index.rst
	@$(RM) -f html-buildstamp
	$(AM_V_GEN) SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir) srctree=$(srcdir) $(SPHINXBUILD) -b html -c $(srcdir)/docs/ $(builddir) $(builddir)/html/dev-doc &> $@

# eg. "src/mmpack-build/common.py" -> "common.rst"
mmpackbuild_doc_tmp = $(notdir $(mmpackbuild_PYTHON))
mmpackbuild_docfile = $(mmpackbuild_doc_tmp:.py=.rst)
$(mmpackbuild_docfile): modules.rst
modules.rst: $(mmpackbuild_PYTHON) $(nodist_mmpackbuild_PYTHON)
	$(AM_V_GEN) SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir) sphinx-apidoc -o . $(srcdir)/src/mmpack-build/

html-buildstamp: sphinx-build.log $(mmpackbuild_docfile)
	@rm -vf $@
	@if [ "$(shell grep -c WARNING: sphinx-build.log)" != "0" ] ; then \
		grep WARNING: sphinx-build.log; \
		false; \
	fi
	@echo
	@echo "Build finished. The HTML pages are in $(builddir)/html."
	@touch $@

dev-doc: html-buildstamp

dev-doc-clean:
	$(RM) -rf html/dev-doc html-buildstamp sphinx-build.log conf.py index.rst $(mmpackbuild_docfile) modules.rst

clean-local: dev-doc-clean

#
# User-level documentation target
#

user_doc_sources += \
	docs/bug.rst \
	docs/configuration.rst \
	docs/conf.py \
	docs/creating-packages.rst \
	docs/devguide.rst \
	docs/faq.rst \
	docs/getting-started.rst \
	docs/index.rst \
	docs/installation.rst \
	docs/introduction.rst \
	docs/usage.rst \
	docs/yaml.rst \
	\
	docs/specs/package-specfile.rst \
	$(eol)

all_user_doc_sources = $(user_doc_sources) $(RST_MAN_PAGES)

user-doc: user-doc-buildstamp
user-doc-buildstamp: $(all_user_doc_sources)
	@rm -vf $@
	mkdir -p $(builddir)/html/user-doc
	$(AM_V_GEN) $(SPHINXBUILD) -b html $(srcdir)/docs/ $(builddir)/html/user-doc
	@touch $@

user-doc-clean:
	 $(RM) -r html/user-doc user-doc-buildstamp

html-local: user-doc
clean-local: user-doc-clean dev-doc-clean
	$(RM) -r html

install-html: html-local
	@mkdir -p $(DESTDIR)$(docdir)/html/
	cd $(builddir)/html && \
		find . -type f -exec $(install_sh_DATA) '{}' $(DESTDIR)$(docdir)/html/'{}' \;

uninstall-html:
	$(RM) -r $(DESTDIR)$(docdir)/html/

endif # BUILD_SPHINXDOC
