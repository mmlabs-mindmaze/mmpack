eol=
ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = autogen.sh TODO.md LICENSE

bin_to_libexecdir=`realpath -m --relative-to=$(bindir) $(libexecdir)`

AM_CPPFLAGS = \
	-I$(srcdir)/src/mmpack \
	-Isrc/mmpack \
	-DSRCDIR=\"$(srcdir)\" \
	-DBUILDDIR=\"$(builddir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	-DEXEEXT=\"$(EXEEXT)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DBIN_TO_LIBEXECDIR=\"$(bin_to_libexecdir)\" \
	$(CHECK_CPPFLAGS) \
	$(LIBARCHIVE_CPPFLAGS) \
	$(ZLIB_CPPFLAGS) \
	$(eol)

if OS_TYPE_WIN32
AM_CPPFLAGS += -DWIN32_LEAN_AND_MEAN
endif

AM_CFLAGS = \
	$(CHECK_CFLAGS) \
	$(LIBARCHIVE_CFLAGS) \
	$(ZLIB_CFLAGS) \
	$(MM_WARNFLAGS) \
	$(eol)

AM_V_RM = $(am__v_RM_$(V))
am__v_RM_ = $(am__v_RM_$(AM_DEFAULT_VERBOSITY))
am__v_RM_0 = -rm -f
am__v_RM_1 = -@rm -vf

bin_PROGRAMS = mmpack
pkglibexec_PROGRAMS = mount-mmpack-prefix
bin_SCRIPTS = \
	src/mmpack-build/mmpack-build \
	src/repository/mmpack-modifyrepo \
	$(eol)

dist_pkgdata_SCRIPTS = \
	src/mmpack/check-dpkg-installed \
	$(eol)
pkgdata_DATA = \
	src/repository/repo.py \
	$(eol)

CLEANFILES = $(bin_SCRIPTS)
EXTRA_DIST += \
	src/mmpack-build/mmpack-build.in \
	src/mmpack-build/settings.py.in \
	src/repository/mmpack-modifyrepo.py.in \
	$(eol)

mmpackbuilddir = $(pkgdatadir)/mmpack_build
mmpackbuild_PYTHON =\
	src/mmpack-build/__init__.py \
	src/mmpack-build/__main__.py \
	src/mmpack-build/base_hook.py \
	src/mmpack-build/binary_package.py \
	src/mmpack-build/common.py \
	src/mmpack-build/decorators.py \
	src/mmpack-build/elf_utils.py \
	src/mmpack-build/errors.py \
	src/mmpack-build/file_utils.py \
	src/mmpack-build/hook_locales.py \
	src/mmpack-build/hook_python.py \
	src/mmpack-build/hook_sharedlib.py \
	src/mmpack-build/hooks_loader.py \
	src/mmpack-build/mmpack_builddep.py \
	src/mmpack-build/mmpack_clean.py \
	src/mmpack-build/mmpack_guess.py \
	src/mmpack-build/mmpack_mksource.py \
	src/mmpack-build/mmpack_pkg_create.py \
	src/mmpack-build/package_info.py \
	src/mmpack-build/pe_utils.py \
	src/mmpack-build/prefix.py \
	src/mmpack-build/provide.py \
	src/mmpack-build/python_depends.py \
	src/mmpack-build/python_dispatch.py \
	src/mmpack-build/python_provides.py \
	src/mmpack-build/source_tarball.py \
	src/mmpack-build/src_package.py \
	src/mmpack-build/syspkg_manager.py \
	src/mmpack-build/syspkg_manager_base.py \
	src/mmpack-build/syspkg_manager_dpkg.py \
	src/mmpack-build/syspkg_manager_pacman_msys2.py \
	src/mmpack-build/mm_version.py \
	src/mmpack-build/workspace.py \
	src/mmpack-build/xdg.py \
	src/mmpack-build/yaml_dumper.py \
	$(eol)

mmpack_db_service = \
	src/repository/mmpack-db.py \
	$(eol)

nodist_mmpackbuild_PYTHON =\
	src/mmpack-build/settings.py \
	$(eol)

BUILT_SOURCES = $(nodist_mmpackbuild_PYTHON) src/mmpack/site.py.inc
CLEANFILES += $(nodist_mmpackbuild_PYTHON) src/mmpack/site.py.inc


.PHONY: pycodestyle
pycodestyle: deployment-test
	pycodestyle --show-source deployment-test$(mmpackbuilddir) \
	                          deployment-test$(pkgdatadir)/*.py \
	                          deployment-test$(bindir)/mmpack-modifyrepo

.PHONY: pylint
pylint: devtools/pylint-config deployment-test
	$(PYLINT) --jobs=0 --disable=locally-disabled --rcfile=$< deployment-test$(mmpackbuilddir) deployment-test$(pkgdatadir)/*.py
	PYTHONPATH=deployment-test$(pkgdatadir) $(PYLINT) --jobs=0 --disable=locally-disabled --rcfile=$< deployment-test$(bindir)/mmpack-modifyrepo

.PHONY: syntax
syntax: pycodestyle pylint
	@echo "$@ target is obsolete, and will be removed soon."
	@echo "Use \"pycodestyle\" and \"pylint\" instead."

mmpack_lib_sources = \
	src/mmpack/site.py.inc \
	src/mmpack/action-solver.c \
	src/mmpack/action-solver.h \
	src/mmpack/binindex.c \
	src/mmpack/binindex.h \
	src/mmpack/binpkg.c \
	src/mmpack/binpkg.h \
	src/mmpack/buffer.c \
	src/mmpack/buffer.h \
	src/mmpack/cmdline.c \
	src/mmpack/cmdline.h \
	src/mmpack/common.h \
	src/mmpack/constraints.h \
	src/mmpack/context.c \
	src/mmpack/context.h \
	src/mmpack/download.c \
	src/mmpack/download.h \
	src/mmpack/indextable.c \
	src/mmpack/indextable.h \
	src/mmpack/install-state.c \
	src/mmpack/install-state.h \
	src/mmpack/mmpack-autoremove.c \
	src/mmpack/mmpack-autoremove.h \
	src/mmpack/mmpack-check-integrity.c \
	src/mmpack/mmpack-check-integrity.h \
	src/mmpack/mmpack-check-sysdep.c \
	src/mmpack/mmpack-check-sysdep.h \
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
	src/mmpack/mmpack-rdepends.c \
	src/mmpack/mmpack-rdepends.h \
	src/mmpack/mmpack-remove.c \
	src/mmpack/mmpack-remove.h \
	src/mmpack/mmpack-repo.c \
	src/mmpack/mmpack-repo.h \
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
	src/mmpack/repo.c \
	src/mmpack/repo.h \
	src/mmpack/settings.c \
	src/mmpack/settings.h \
	src/mmpack/sha256.c \
	src/mmpack/sha256.h \
	src/mmpack/srcindex.c \
	src/mmpack/srcindex.h \
	src/mmpack/strchunk.h \
	src/mmpack/strchunk.c \
	src/mmpack/strlist.h \
	src/mmpack/strlist.c \
	src/mmpack/sysdeps.c \
	src/mmpack/sysdeps.h \
	src/mmpack/tar.c \
	src/mmpack/tar.h \
	src/mmpack/utils.c \
	src/mmpack/utils.h \
	src/mmpack/xx-alloc.h \
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

licensedir = $(pkgdatadir)/common-licenses
dist_license_DATA = \
	common-licenses/Apache-2.0 \
	common-licenses/Artistic \
	common-licenses/BSD \
	common-licenses/CC0-1.0 \
	common-licenses/GFDL-1.2 \
	common-licenses/GFDL-1.3 \
	common-licenses/GPL-1 \
	common-licenses/GPL-2 \
	common-licenses/GPL-3 \
	common-licenses/LGPL-2 \
	common-licenses/LGPL-2.1 \
	common-licenses/LGPL-3 \
	common-licenses/MPL-1.1 \
	common-licenses/MPL-2.0 \
	$(eol)


dist_pkgdata_DATA = \
	src/mmpack/prompt.bash \
	$(eol)

buildscriptsdir = $(mmpackbuilddir)/buildscripts
dist_buildscripts_DATA = \
	src/mmpack-build/build-autotools \
	src/mmpack-build/build-cmake \
	src/mmpack-build/build-makefile \
	src/mmpack-build/build-meson \
	src/mmpack-build/build-python \
	$(eol)

mmpack_LDADD = \
	$(CURL_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(ZLIB_LIBS) \
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


# write the 2nd @ between [] in order to prevent autotools replacing it
# BEFORE the sed call
%.py : %.py.in Makefile
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(SED) -e 's,@pydir_to_pkgdatadir[@],$(shell realpath -m --relative-to=$(mmpackbuilddir) $(pkgdatadir)),g' \
		-e 's,@pydir_to_libexecdir[@],$(shell realpath -m --relative-to=$(mmpackbuilddir) $(libexecdir)),g' \
		-e 's,@pydir_to_bindir[@],$(shell realpath -m --relative-to=$(mmpackbuilddir) $(bindir)),g' \
		-e 's,@package_version[@],$(PACKAGE_VERSION),g' \
		-e 's,@exeext[@],$(EXEEXT),g' < $< > $@

%.inc : % Makefile
	$(AM_V_GEN)python3 $(srcdir)/devtools/convert-to-cstr.py -o $@ $<

src/repository/mmpack-modifyrepo: src/repository/mmpack-modifyrepo.py.in Makefile
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(SED) -e 's,@pkgdatadir[@],$(pkgdatadir),g' \
		-e 's,@bindir[@],$(bindir),g' \
		< $< > $@

src/mmpack-build/mmpack-build: src/mmpack-build/mmpack-build.in
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(SED) -e 's,@bindir_to_pkgdatadir[@],$(shell realpath -m --relative-to=$(bindir) $(pkgdatadir)),g' \
		-e 's,@python_path@,$(PYTHON),g' \
	        < $< > $@

#
# DEVEL TARGETS
#
# Those should only make sense as helpers in a development context

user_doc_sources =
.PHONY: spelling
spelling: $(mmpackbuild_PYTHON) $(filter-out %.inc,$(mmpack_lib_sources)) $(RST_MAN_PAGES) \
		$(user_doc_sources) $(mmpack_db_service)
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
		$(filter-out %.inc,$(mmpack_SOURCES)) \
		$(mount_mmpack_prefix_SOURCES)

# check all the source files for correct coding style
.PHONY: checkstyle
checkstyle: devtools/uncrustify.cfg pycodestyle
	cd $(srcdir) && uncrustify -c devtools/uncrustify.cfg -l C --check \
	   $(mmpack_SOURCES) \
	   $(mount_mmpack_prefix_SOURCES)

#
# TESTS RULES
#

deployment-test: $(mmpack_SOURCES) \
		$(mount_mmpack_prefix_SOURCES) $(mmpackbuild_PYTHON) \
		$(nodist_mmpackbuild_PYTHON) \
		$(dist_pkgdata_SCRIPTS) \
		$(pkgdata_DATA) \
		$(bin_SCRIPTS)
	DESTDIR=$(shell readlink -f $@) $(MAKE) install
	@touch $@

deployment-test-clean:
	@$(RM) -rvf deployment-test

test-repo.timestamp: tests/create-test-repo.sh deployment-test
	$(AM_V_GEN)$< $(builddir)/deployment-test $(builddir)/test-repo $(bindir)
	@touch $@

test-sysrepo.timestamp: tests/create-test-sysrepo.sh tests/test-packages
	$(AM_V_GEN)$< $(builddir)/test-sysrepo true
	@touch $@

test-packages.timestamp: tests/create-test-pkg.sh tests/test-packages
	$(AM_V_GEN)$< $(builddir)/test-packages true
	@touch $@

noinst_PYTHON = tests/pytests.py

check_DATA = deployment-test test-repo.timestamp test-packages.timestamp test-sysrepo.timestamp

TESTS = \
	tests/python-unittests.tap \
	tests/smoke-tests/launch_test_mmpack-build_autotools.sh \
	tests/smoke-tests/test_mmpack-build_ghost.sh \
	tests/smoke-tests/test_mmpack_autoremove.sh \
	tests/smoke-tests/test_mmpack_check-integrity.sh \
	tests/smoke-tests/test_mmpack_download.sh \
	tests/smoke-tests/test_mmpack_install.sh \
	tests/smoke-tests/test_mmpack_list.sh \
	tests/smoke-tests/test_mmpack_manually_installed.sh \
	tests/smoke-tests/test_mmpack_mkprefix.sh \
	tests/smoke-tests/test_mmpack_rdepends.sh \
	tests/smoke-tests/test_mmpack_remove.sh \
	tests/smoke-tests/test_mmpack_repo.sh \
	tests/smoke-tests/test_mmpack_search.sh \
	tests/smoke-tests/test_mmpack_show.sh \
	tests/smoke-tests/test_mmpack_source.sh \
	tests/smoke-tests/test_mmpack_upgrade.sh \
	unittests.tap \
	$(eol)

dist_check_SCRIPTS = tests/python-unittests.tap
check_PROGRAMS = \
	unittests.tap \
	$(eol)

# The variable _MMPACK_TEST_PREFIX is needed by mmpack and mmpack-build to find
# everything in the right place during the tests (that require a local
# installation of mmpack project).
TESTS_ENVIRONMENT = _MMPACK_TEST_PREFIX=$(abs_builddir)/deployment-test PKGDATADIR=$(pkgdatadir) \
					SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir) PREFIX=$(prefix) REPO=$(abs_builddir)/test-repo
TEST_EXTENSIONS = .test .tap .sh

if TAP_SUPPORT_IN_CHECK
AM_CPPFLAGS += -DCHECK_SUPPORT_TAP
TAP_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(srcdir)/build-aux/tap-driver.sh
else
TAP_LOG_DRIVER = $(LOG_DRIVER)
endif

EXTRA_DIST += \
	tests/create-mpks \
	tests/run-test-mpkrepo \
	tests/mmpack-config.yaml \
	tests/pydata/ \
	tests/test_common.py \
	tests/test_file_utils.py \
	tests/test_hook_python.py \
	tests/test_readme_parsing.py \
	tests/test_version.py \
	tests/binary-indexes \
	tests/source-indexes \
	$(eol)

unittests_tap_SOURCES = \
	$(mmpack_lib_sources) \
	tests/binindex_test.c \
	tests/config_test.c \
	tests/dependencies_test.c \
	tests/indextable_test.c \
	tests/misc_test.c \
	tests/sha_test.c \
	tests/srcindex_test.c \
	tests/testcases.h \
	tests/unittests.c \
	tests/version_test.c \
	$(eol)

unittests_tap_LDADD = \
	$(CHECK_LIBS) \
	$(CURL_LIB) \
	$(LIBARCHIVE_LIBS) \
	$(ZLIB_LIBS) \
	$(MMLIB_LIB) \
	$(YAML_LIB) \
	$(eol)

completiondir = $(datadir)/bash-completion/completions
dist_completion_DATA = \
	data/shell-completions/bash/mmpack \
	data/shell-completions/bash/mmpack-build \
	data/shell-completions/bash/mmpack-modifyrepo \
	$(eol)

#
# DOC RULES
#

RST_MAN1_PAGES = \
	docs/man/1/mmpack.rst \
	docs/man/1/mmpack_autoremove.rst \
	docs/man/1/mmpack-build.rst \
	docs/man/1/mmpack-build_builddep.rst \
	docs/man/1/mmpack-build_clean.rst \
	docs/man/1/mmpack-build_guess.rst \
	docs/man/1/mmpack-build_mksource.rst \
	docs/man/1/mmpack-build_pkg-create.rst \
	docs/man/1/mmpack_check-integrity.rst \
	docs/man/1/mmpack_download.rst \
	docs/man/1/mmpack_fix-broken.rst \
	docs/man/1/mmpack_install.rst \
	docs/man/1/mmpack_list.rst \
	docs/man/1/mmpack_mkprefix.rst \
	docs/man/1/mmpack-modifyrepo.rst \
	docs/man/1/mmpack_provides.rst \
	docs/man/1/mmpack_rdepends.rst \
	docs/man/1/mmpack_remove.rst \
	docs/man/1/mmpack_repo.rst \
	docs/man/1/mmpack_run.rst \
	docs/man/1/mmpack_search.rst \
	docs/man/1/mmpack_show.rst \
	docs/man/1/mmpack_source.rst \
	docs/man/1/mmpack_update.rst \
	docs/man/1/mmpack_upgrade.rst \
	$(eof)
RST_MAN5_PAGES = \
	docs/man/5/mmpack-config.rst \
	$(eof)
EXTRA_DIST += $(RST_MAN1_PAGES) $(RST_MAN5_PAGES)

nodist_man_MANS = $(RST_MAN1_PAGES:.rst=.1) $(RST_MAN5_PAGES:.rst=.5)
CLEANFILES += $(nodist_man_MANS)

# rst2man is part of the docutils package which is a dependency of sphinx
%.1: %.rst
	$(AM_V_at) $(MKDIR_P) docs/man/1
	$(AM_V_GEN) $(RST2MAN) $< $@
%.5: %.rst
	$(AM_V_at) $(MKDIR_P) docs/man/5
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
	docs/specs/package-sources-strap.rst \
	docs/specs/repo.rst \
	$(eol)

all_user_doc_sources = $(user_doc_sources) $(RST_MAN_PAGES)

user-doc: user-doc-buildstamp
user-doc-buildstamp: $(all_user_doc_sources)
	@rm -vf $@
	mkdir -p $(builddir)/html/user-doc
	$(AM_V_GEN) $(SPHINXBUILD) \
		-D version=$(PACKAGE_VERSION) \
		-b html $(srcdir)/docs/ $(builddir)/html/user-doc
	@touch $@

user-doc-clean:
	 $(RM) -r html/user-doc user-doc-buildstamp

html-local: user-doc
clean-local: user-doc-clean dev-doc-clean
	$(RM) -r html test-sysrepo* test-packages*

install-html: html-local
	@mkdir -p $(DESTDIR)$(docdir)/html/
	cd $(builddir)/html && \
		find . -type f -exec $(install_sh_DATA) '{}' $(DESTDIR)$(docdir)/html/'{}' \;

uninstall-html:
	$(RM) -r $(DESTDIR)$(docdir)/html/

endif # BUILD_SPHINXDOC
