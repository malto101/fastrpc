SUBDIRS = inc src test files Docs/manpages

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST = $(shell find $(srcdir)/test/base_test -type f \
	-not -path '$(srcdir)/test/base_test/builddir/*' \
	-not -name .git)

BASE_TEST_SRCDIR = $(abs_top_srcdir)/test/base_test
BASE_TEST_BUILDDIR = $(abs_top_builddir)/test/base_test/builddir
BASE_TEST_LIBRARY = $(abs_top_builddir)/src/.libs/libcdsprpc.so
BASE_TEST_CMAKE = @CMAKE@
BASE_TEST_QAIC = @QAIC_EXECUTABLE@
BASE_TEST_HEXAGON_CLANG = @HEXAGON_CLANG@
BASE_TEST_HEXAGON_LD = @HEXAGON_LD@
BASE_TEST_ANDROID_NDK = @ANDROID_NDK_HOME@
BASE_TEST_DSP_ARCH = @BASE_TEST_DSP_ARCH@
BASE_TEST_PLATFORM = @BASE_TEST_PLATFORM@

.PHONY: base-test base-test-clean src/libcdsprpc.la

if BASE_TEST_ENABLED

all-local: base-test

base-test: src/libcdsprpc.la
	$(MKDIR_P) "$(BASE_TEST_BUILDDIR)"
	CC="$(CC)" CXX="$(CXX)" CPP="$(CPP)" AS="$(AS)" \
	AR="$(AR)" RANLIB="$(RANLIB)" LD="$(LD)" STRIP="$(STRIP)" \
	CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" \
	CPPFLAGS="$(CPPFLAGS)" LDFLAGS="$(LDFLAGS)" \
	"$(BASE_TEST_CMAKE)" -S "$(BASE_TEST_SRCDIR)" \
		-B "$(BASE_TEST_BUILDDIR)" \
		-DTARGET_PLATFORM="$(BASE_TEST_PLATFORM)" \
		-DFASTRPC_ROOT="$(abs_top_srcdir)" \
		-DFASTRPC_INCLUDE_DIR="$(abs_top_srcdir)/inc" \
		-DFASTRPC_LIBRARY="$(BASE_TEST_LIBRARY)" \
		-DQAIC_EXECUTABLE="$(BASE_TEST_QAIC)" \
		-DHEXAGON_CLANG="$(BASE_TEST_HEXAGON_CLANG)" \
		-DHEXAGON_LD="$(BASE_TEST_HEXAGON_LD)" \
		-DANDROID_NDK_HOME="$(BASE_TEST_ANDROID_NDK)" \
		-DDSP_ARCH="$(BASE_TEST_DSP_ARCH)"
	"$(BASE_TEST_CMAKE)" --build "$(BASE_TEST_BUILDDIR)"

# Recursive Automake builds do not expose individual subdirectory targets at
# the top level.  Delegate the library dependency to src/ explicitly so a
# direct or parallel `make base-test` cannot race CMake configuration.
src/libcdsprpc.la:
	+$(MAKE) -C "$(abs_top_builddir)/src" libcdsprpc.la

endif

.PHONY: base-test-clean

base-test-clean:
	$(AM_V_at)$(RM) -r "$(BASE_TEST_BUILDDIR)"

clean-local: base-test-clean
distclean-local: base-test-clean

# The CMake project has its own dependency graph and must not be installed by
# the FastRPC library build.
