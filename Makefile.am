SUBDIRS = inc src test files Docs/manpages

ACLOCAL_AMFLAGS = -I m4

COVERAGE_INFO = fastrpc-coverage.info
COVERAGE_REPORT_DIR = fastrpc-coverage

if CODE_COVERAGE_ENABLED
coverage: all
	$(LCOV) --capture --initial \
		--directory "$(abs_top_builddir)/src" \
		--base-directory "$(abs_top_srcdir)/src" \
		--gcov-tool "$(GCOV)" \
		--output-file "$(COVERAGE_INFO).base"
	$(LCOV) --capture \
		--directory "$(abs_top_builddir)/src" \
		--base-directory "$(abs_top_srcdir)/src" \
		--gcov-tool "$(GCOV)" \
		--output-file "$(COVERAGE_INFO).run"
	$(LCOV) --add-tracefile "$(COVERAGE_INFO).base" \
		--add-tracefile "$(COVERAGE_INFO).run" \
		--output-file "$(COVERAGE_INFO).tmp"
	$(LCOV) --extract "$(COVERAGE_INFO).tmp" '*/src/*' \
		--output-file "$(COVERAGE_INFO)"
	$(GENHTML) "$(COVERAGE_INFO)" \
		--output-directory "$(COVERAGE_REPORT_DIR)" \
		--title "FastRPC code coverage" \
		--legend --show-details
	$(RM) "$(COVERAGE_INFO).base" "$(COVERAGE_INFO).run" \
		"$(COVERAGE_INFO).tmp"
	@echo "Coverage report: $(abs_top_builddir)/$(COVERAGE_REPORT_DIR)/index.html"
else
coverage:
	@echo "Reconfigure with --enable-code-coverage to generate a coverage report."
	@false
endif

CLEAN_LOCAL_DEPS = coverage-clean

coverage-clean:
	-find "$(abs_top_builddir)" -name '*.gcda' -delete
	$(RM) "$(COVERAGE_INFO)" "$(COVERAGE_INFO).base" \
		"$(COVERAGE_INFO).run" "$(COVERAGE_INFO).tmp"
	$(RM) -r "$(COVERAGE_REPORT_DIR)"

clean-local: $(CLEAN_LOCAL_DEPS)

.PHONY: coverage coverage-clean base-test base-test-stage base-test-clean src/libcdsprpc.la

BASE_TEST_SRCDIR = $(abs_top_srcdir)/test/base_test
BASE_TEST_BUILDDIR = $(abs_top_builddir)/test/base_test/builddir
BASE_TEST_STAGEDIR = $(BASE_TEST_BUILDDIR)/_stage
BASE_TEST_CMAKE = @CMAKE@
BASE_TEST_QAIC = @QAIC_EXECUTABLE@
BASE_TEST_HEXAGON_CLANG = @HEXAGON_CLANG@
BASE_TEST_HEXAGON_LD = @HEXAGON_LD@
BASE_TEST_DSP_ARCH = @BASE_TEST_DSP_ARCH@
BASE_TEST_TOOLCHAIN_FILE = @BASE_TEST_TOOLCHAIN_FILE@
BASE_TEST_CMAKE_EXTRA_ARGS = @BASE_TEST_CMAKE_EXTRA_ARGS@
# Mirrors test/Makefile.am's testdspdir layout for fastrpc_test.
BASE_TEST_DSPDIR = $(datadir)/fastrpc_base_test/v$(BASE_TEST_DSP_ARCH)

# Ship test/base_test (including the vendor/unity submodule contents, when
# checked out) via a dist-hook instead of a $(shell find ...) EXTRA_DIST
# assignment.  The recipe runs under $(SHELL), avoiding GNU make's parse-time
# $(shell ...) extension.
dist-hook:
	@basetestdir=`cd '$(distdir)' && pwd`/test/base_test; \
	cd '$(BASE_TEST_SRCDIR)' && \
	find . -type f ! -path './builddir/*' ! -name '.git' | \
	while read -r f; do \
		$(MKDIR_P) "$$basetestdir/$$(dirname "$$f")"; \
		cp -p "$$f" "$$basetestdir/$$f"; \
	done

if BASE_TEST_ENABLED

all-local: base-test

base-test: base-test-stage
	$(MKDIR_P) "$(BASE_TEST_BUILDDIR)"
	@# CC/CXX may be a command with mandatory flags (e.g. Yocto's
	@# "aarch64-poky-linux-gcc -mcpu=... --sysroot=...").  CMake rejects a
	@# compiler path containing spaces but accepts a ;-list whose first entry
	@# is the compiler and the rest its mandatory arguments, so pass the same
	@# command unchanged, just ;-separated.  CMAKE_LINKER must be a plain path
	@# (CMake only inspects it; linking goes through the compiler driver), so
	@# only the tool itself is taken from LD.
	set -f; \
	base_test_cc=; for w in $(CC); do base_test_cc="$${base_test_cc:+$$base_test_cc;}$$w"; done; \
	base_test_cxx=; for w in $(CXX); do base_test_cxx="$${base_test_cxx:+$$base_test_cxx;}$$w"; done; \
	set -- $(LD); base_test_ld="$${1:-}"; \
	"$(BASE_TEST_CMAKE)" -S "$(BASE_TEST_SRCDIR)" \
		-B "$(BASE_TEST_BUILDDIR)" \
		-DCMAKE_TOOLCHAIN_FILE="$(BASE_TEST_TOOLCHAIN_FILE)" \
		$(BASE_TEST_CMAKE_EXTRA_ARGS) \
		-DFASTRPC_PREFIX="$(BASE_TEST_STAGEDIR)" \
		-UFASTRPC_LIBRARY -UFASTRPC_ROOT -UFASTRPC_INCLUDE_DIR \
		-DQAIC_EXECUTABLE="$(BASE_TEST_QAIC)" \
		-DHEXAGON_CLANG="$(BASE_TEST_HEXAGON_CLANG)" \
		-DHEXAGON_LD="$(BASE_TEST_HEXAGON_LD)" \
		-DDSP_ARCH="$(BASE_TEST_DSP_ARCH)" \
		-DCMAKE_C_COMPILER="$$base_test_cc" \
		-DCMAKE_CXX_COMPILER="$$base_test_cxx" \
		-DCMAKE_AR="$(AR)" \
		-DCMAKE_RANLIB="$(RANLIB)" \
		-DCMAKE_LINKER="$$base_test_ld" \
		-DCMAKE_STRIP="$(STRIP)" \
		-DCMAKE_C_FLAGS="$(CPPFLAGS) $(CFLAGS)" \
		-DCMAKE_CXX_FLAGS="$(CPPFLAGS) $(CXXFLAGS)" \
		-DCMAKE_EXE_LINKER_FLAGS="$(LDFLAGS)" \
		-DCMAKE_INSTALL_PREFIX="$(prefix)" \
		-DCMAKE_INSTALL_BINDIR="$(bindir)" \
		-DCMAKE_INSTALL_DATADIR="$(datadir)" \
		-DBASE_TEST_INSTALL_DSPDIR="$(BASE_TEST_DSPDIR)"
	"$(BASE_TEST_CMAKE)" --build "$(BASE_TEST_BUILDDIR)"

base-test-stage: src/libcdsprpc.la
	$(MKDIR_P) "$(BASE_TEST_STAGEDIR)"
	+$(MAKE) -C "$(abs_top_builddir)/inc" install-fastrpc_includeHEADERS DESTDIR= \
		prefix="$(BASE_TEST_STAGEDIR)" exec_prefix="$(BASE_TEST_STAGEDIR)" \
		includedir="$(BASE_TEST_STAGEDIR)/include"
	+$(MAKE) -C "$(abs_top_builddir)/src" install-libLTLIBRARIES DESTDIR= \
		lib_LTLIBRARIES=libcdsprpc.la \
		prefix="$(BASE_TEST_STAGEDIR)" exec_prefix="$(BASE_TEST_STAGEDIR)" \
		libdir="$(BASE_TEST_STAGEDIR)/lib"

# Recursive Automake builds do not expose individual subdirectory targets at
# the top level.  Delegate the library dependency to src/ explicitly so a
# direct or parallel `make base-test` cannot race CMake configuration.
src/libcdsprpc.la:
	+$(MAKE) -C "$(abs_top_builddir)/src" libcdsprpc.la

# Install only the base_test component; vendored Unity's own install rules
# (static library, headers, CMake package files) are skipped.  cmake --install
# honours DESTDIR from the environment.
install-exec-local:
	DESTDIR="$(DESTDIR)" "$(BASE_TEST_CMAKE)" --install "$(BASE_TEST_BUILDDIR)" \
		--component base_test

uninstall-local:
	$(RM) "$(DESTDIR)$(bindir)/fastrpc_base_test" \
		"$(DESTDIR)$(BASE_TEST_DSPDIR)/libfastrpc_base_test_skel.so"
	-rmdir "$(DESTDIR)$(BASE_TEST_DSPDIR)" \
		"$(DESTDIR)$(datadir)/fastrpc_base_test" 2>/dev/null

base-test-clean:
	$(AM_V_at)$(RM) -r "$(BASE_TEST_BUILDDIR)"

CLEAN_LOCAL_DEPS += base-test-clean
distclean-local: base-test-clean

endif
