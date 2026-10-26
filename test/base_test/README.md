# FastRPC base_test

`base_test` is the CMake/Unity test suite for the FastRPC library. The test
runner is an AArch64 host executable. It opens a FastRPC session on the target
DSP and calls the generated interface in `idl/fastrpc_test.idl`; it is not a
host-side emulator.

The snapshot documented here (`b163af5`) contains two end-to-end suites. The
suite is intentionally opt-in for the normal Autotools build. The normal
FastRPC build does not require CMake, QAIC, Unity, or a Hexagon compiler.

## Feature matrix

| Suite | Group | Cases in this snapshot | Tags | DSP artifact |
|---|---|---|---|---|
| Calculator E2E | `CalculatorE2E` | `SumAndMaximum`, `EmptyInputsAreRejected` | `Calculator`, `e2e`, `positive`/`negative`, `Remote` | `libfastrpc_test_skel.so` |
| HAP example E2E | `HapExampleE2E` | `FarfRuntimeLoggingCanBeConfigured` | `HapExample`, `e2e`, `positive`, `Remote`, `FARF` | `libfastrpc_test_skel.so` |
| Feature | none registered | none | — | — |
| Unit | none registered | none | — | — |

Both E2E suites use the same generated stub and skeleton. Calculator exercises
sum, maximum, and invalid-input handling. The HAP example configures FARF
runtime logging and emits sample messages. A target HAP/Hexagon SDK runtime is
needed for the HAP operation on the DSP.

## Prerequisites

### Host and source prerequisites

Install or provide the following before configuring the suite.

| Prerequisite | Why it is needed |
|---|---|
| AArch64 Linux toolchain (or an AArch64 Linux machine) | Compiles the host runner and `libcdsprpc` for the target. |
| CMake 3.21 or newer | Configures the suite. |
| Ninja (recommended) | Fast, reproducible CMake builds. |
| QAIC executable | Generates the test header, CPU stub, and DSP skeleton from the IDL. |
| Unity submodule | Supplies the test framework. |
| `libyaml` and `libbsd` for non-Android Autotools builds | Required when building the FastRPC library on Linux. |
| Hexagon SDK/LLVM toolchain (`HEXAGON_CLANG`, `HEXAGON_LD`) | Compiles and links the required DSP skeleton. |

Initialize the submodule from the repository root:

```bash
git submodule update --init --recursive
```

### Complete device build prerequisites

A host runner alone is not a complete device build. For a build that can be
deployed and execute the E2E cases, provide all of the following:

- A built AArch64 `libcdsprpc.so` (`src/.libs/libcdsprpc.so` in an in-tree
  build) and the matching FastRPC headers.
- An executable QAIC compiler, passed as `-DQAIC_EXECUTABLE=...`.
- An executable Hexagon compiler and linker. Set both explicitly:
  `-DHEXAGON_CLANG=/path/to/hexagon-clang` and
  `-DHEXAGON_LD=/path/to/ld.lld` (or the Hexagon linker supplied by the SDK).
  They must support the selected `DSP_ARCH`.
- A `DSP_ARCH` value supported by the device. The default is `73`; select the
  device's actual architecture rather than copying the default.
- A target with the FastRPC kernel/user drivers, the required daemon(s), a
  running remoteproc, and an accessible `/dev/fastrpc-*` node.
- A deployment location visible to the DSP loader for
  `libfastrpc_test_skel.so`, plus a target runtime location for the host
  executable and `libcdsprpc.so`.
- Permission to use the selected domain and either an unsigned-PD policy
  (`-u 1`, the default) or a correctly signed skeleton (`-u 0`).

Treat a configuration that cannot resolve `HEXAGON_CLANG` or `HEXAGON_LD` as a
failed complete-device build. A host executable without
`libfastrpc_test_skel.so` cannot run the DSP calls, even if the host target
itself compiled successfully.

## Quickstart

There are two supported build paths. In both paths, the target is AArch64.
The commands below use Linux GNU as the concrete target; use a native AArch64
machine with `./gitcompile` instead of `--host` when building directly on an
AArch64 Linux machine.

### Integrated build from the repository root

This path builds the FastRPC library first and then invokes the CMake suite.
It is a cross-build for `aarch64-linux-gnu`, not a native x86 build:

```bash
cd /path/to/fastrpc
git submodule update --init --recursive
./gitcompile --host=aarch64-linux-gnu \
  --enable-base-test \
  --with-qaic=/path/to/qaic \
  --with-dsp-arch=73
make base-test
```

The normal cross-compiler environment (`CC`, `CXX`, `AR`, `RANLIB`, `LD`, and
`STRIP`) must be set as described in the root [build instructions](../../README.md).
For a native AArch64 Linux checkout, run `./gitcompile --enable-base-test ...`
without `--host` and then run `make base-test`.

The integrated target forwards the FastRPC root, headers, library, QAIC path,
platform, and `DSP_ARCH` to CMake. Its outputs are relative to the repository
root at `test/base_test/builddir/bin/` and
`test/base_test/builddir/idl/generated/`. Use `make base-test-clean` to remove
that CMake build directory. Run `make distclean` before changing compilers or
target platforms so stale CMake cache entries are not reused. The integrated
configuration also requires `HEXAGON_CLANG` and `HEXAGON_LD` through CMake's
tool lookup or cache; set them explicitly when the Hexagon SDK tools are not on
`PATH`.

For a complete device build, use the standalone command below so the Hexagon
compiler and linker can be supplied explicitly.

### Standalone build

Build the FastRPC library first from the repository root, then change into
`test/base_test` before invoking CMake. All paths in this section are relative
to `test/base_test` unless an external tool is shown as an absolute path.

```bash
cd /path/to/fastrpc
./gitcompile --host=aarch64-linux-gnu

git submodule update --init --recursive
cd test/base_test

cmake -B builddir -G Ninja \
  -DTARGET_PLATFORM=linux \
  -DFASTRPC_ROOT=../.. \
  -DFASTRPC_INCLUDE_DIR=../../inc \
  -DFASTRPC_LIBRARY=../../src/.libs/libcdsprpc.so \
  -DQAIC_EXECUTABLE=/path/to/qaic \
  -DHEXAGON_CLANG=/path/to/hexagon-clang \
  -DHEXAGON_LD=/path/to/ld.lld \
  -DDSP_ARCH=73
cmake --build builddir
```

On a native AArch64 Linux machine, build the library with `./gitcompile` and
use the same `cd test/base_test` and CMake commands. When using a cross GCC
rather than the standalone Clang defaults, export `CC`, `AR`, `RANLIB`, `LD`,
and `STRIP` for the AArch64 toolchain before configuring CMake.

The Android target remains available through the existing platform file; it
requires an AArch64 Android NDK. No additional deployment platforms are
introduced by this suite:

```bash
cd test/base_test
cmake -B builddir -G Ninja \
  -DTARGET_PLATFORM=android \
  -DANDROID_NDK_HOME=/path/to/android-ndk \
  -DFASTRPC_ROOT=../.. \
  -DFASTRPC_INCLUDE_DIR=../../inc \
  -DFASTRPC_LIBRARY=../../src/.libs/libcdsprpc.so \
  -DQAIC_EXECUTABLE=/path/to/qaic \
  -DHEXAGON_CLANG=/path/to/hexagon-clang \
  -DHEXAGON_LD=/path/to/ld.lld \
  -DDSP_ARCH=73
cmake --build builddir
```

### CMake options

| Option | Default | Description |
|---|---|---|
| `TARGET_PLATFORM` | `linux` | Existing target platform: `linux` or `android`. |
| `FASTRPC_ROOT` | `../..` in an in-tree checkout | FastRPC source root. |
| `FASTRPC_INCLUDE_DIR` | derived | Directory containing `remote.h` and other FastRPC headers. |
| `FASTRPC_LIBRARY` | derived | Path to `libcdsprpc.so`. |
| `FASTRPC_PREFIX` | empty | Alternative installed FastRPC prefix. |
| `QAIC_EXECUTABLE` | required | Executable QAIC path. |
| `HEXAGON_CLANG` | tool lookup | Hexagon compiler for the DSP objects; set explicitly for a device build. |
| `HEXAGON_LD` | tool lookup | Hexagon linker for `libfastrpc_test_skel.so`; set explicitly for a device build. |
| `DSP_ARCH` | `73` | Decimal Hexagon architecture number, for example `68`, `73`, or `75`. |
| `ANDROID_NDK_HOME` | empty | Android NDK root when `TARGET_PLATFORM=android`. |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`. |

The configure step validates the FastRPC headers, library, QAIC, and platform
inputs. QAIC output is generated under `builddir/idl/generated/`; do not edit
those files by hand.

## Build outputs and deployment

After `cmake --build builddir`, the deployable files are in `builddir/bin/`:

- `fastrpc_test` is the AArch64 CPU-side test runner. It contains the generated
  CPU stub and links to `libcdsprpc.so`.
- `libfastrpc_test_skel.so` is the required Hexagon DSP-side skeleton and test
  implementation. CMake fails during configuration when the complete Hexagon
  compiler/linker toolchain is unavailable or cannot target `DSP_ARCH`.

For a target with an `adb` deployment path, one minimal example is:

```bash
adb push builddir/bin/fastrpc_test /data/local/tmp/
adb push builddir/bin/libfastrpc_test_skel.so /data/local/tmp/
# Push the matching libcdsprpc.so too when it is not installed on the target.
adb push ../../src/.libs/libcdsprpc.so /data/local/tmp/
adb shell 'LD_LIBRARY_PATH=/data/local/tmp DSP_LIBRARY_PATH=/data/local/tmp \
  /data/local/tmp/fastrpc_test -d 3'
```

Use the target's normal DSP library search path instead of
`DSP_LIBRARY_PATH` when the device policy requires it. The skeleton, FastRPC
library, host binary, domain, and `DSP_ARCH` must all belong to the same build.

## Running the tests

Run commands from `test/base_test`; the binary is `builddir/bin/fastrpc_test`.

```bash
# Run on every running domain discovered from remoteproc and /dev/fastrpc-*.
./builddir/bin/fastrpc_test

# Run only on CDSP (domain 3); repeat -d to select multiple domains.
./builddir/bin/fastrpc_test -d 3

# List metadata without opening a DSP session or discovering domains.
./builddir/bin/fastrpc_test --list-tests
./builddir/bin/fastrpc_test --list-groups
./builddir/bin/fastrpc_test --list-tags

# Select tests by tags, repeat either option as needed.
./builddir/bin/fastrpc_test --any-tags Calculator --any-tags FARF
./builddir/bin/fastrpc_test --all-tags e2e --all-tags positive

# Select a Unity group/name and repeat selected cases.
./builddir/bin/fastrpc_test -g CalculatorE2E -n SumAndMaximum
./builddir/bin/fastrpc_test -r 2
```

### CLI reference

The base-test runner accepts the following options. Options not listed here
are rejected; the legacy `test/fastrpc_test` options are a different runner.

| Option | Meaning |
|---|---|
| `-d DOMAIN` | Select one known DSP domain. Repeat to select several; duplicate IDs are ignored. With no `-d`, running domains are discovered. |
| `-u 0\|1` | `1` enables unsigned-PD control before opening a handle (default); `0` skips that control for a signed skeleton. |
| `-l tests\|groups\|tags` | List selected test cases, groups, or tags. This is equivalent to the corresponding long option. |
| `--list-tests` | Print sorted `Group.Case` entries and exit. |
| `--list-groups` | Print sorted selected group names and exit. |
| `--list-tags` | Print all registered tags and exit. |
| `--any-tags TAG` | OR filter: select a case matching at least one supplied tag. Repeatable, up to 32 values. |
| `--all-tags TAG` | AND filter: select a case matching every supplied tag. Repeatable, up to 32 values. |
| `--silent` | Suppress output for passing cases; failures and skips remain visible. |
| `-g GROUP` | Select an exact Unity group name. |
| `-G TEXT` | Select groups whose names contain `TEXT`. |
| `-n CASE` | Select an exact Unity case name. |
| `-N TEXT` | Select cases whose names contain `TEXT`. |
| `-r COUNT` | Repeat the selected cases `COUNT` times; `COUNT` must be greater than zero. |
| `-v` | Enable Unity verbose output. |
| `-s` | Enable Unity silent output. Prefer `--silent` when using the base-test output summary. |
| `-h`, `--help` | Print runner help and exit. |

Tag matching is case-sensitive. When both tag filters are present, a test must
pass both filters. Listing commands use metadata only and never probe the DSP.
A selected test without a domain declaration supports every known domain; a
declared domain list is checked before the test opens a session.

### Domain reference

These are the domain IDs recognized by this snapshot. Automatic discovery
requires both a running matching remoteproc and its character device.

| ID | FastRPC name | Common remoteproc name | Device node |
|---:|---|---|---|
| `0` | ADSP (`adsp`) | `adsp` | `/dev/fastrpc-adsp` |
| `1` | MDSP (`mdsp`) | `mdsp` | `/dev/fastrpc-mdsp` |
| `2` | SDSP (`sdsp`) | `sdsp` or `slpi` | `/dev/fastrpc-sdsp` |
| `3` | CDSP (`cdsp`) | `cdsp` | `/dev/fastrpc-cdsp` |
| `4` | CDSP1 (`cdsp1`) | `cdsp1` | `/dev/fastrpc-cdsp1` |
| `5` | GDSP0 (`gdsp0`) | `gpdsp0` or `gdsp0` | `/dev/fastrpc-gdsp0` |
| `6` | GDSP1 (`gdsp1`) | `gpdsp1` or `gdsp1` | `/dev/fastrpc-gdsp1` |

A target need not provide every row. An explicit `-d` avoids the discovery
scan, but it does not bypass a missing device node, stopped remoteproc, access
permission, or unavailable DSP service. Domain IDs outside this table are
invalid for this snapshot.

### Expected status

Each selected case contributes one of these outcomes:

- **PASS** — setup, test body, and teardown completed without a Unity failure.
- **SKIP** — the selected domain is not supported by the case metadata, or the
  test setup cannot open the optional DSP service. Hardware absence is reported
  as a skip by the E2E fixtures.
- **FAIL** — an assertion, FastRPC call, teardown, configuration, or test
  execution failed. A malformed command or inability to discover any domain
  exits before test execution and is also an error.

The summary reports total, passed, failed, and skipped counts. A run with no
failures exits successfully; invalid configuration, no discovered domain, or a
failed test exits nonzero. Use the printed error code and the case name when
triaging a failure.

## Troubleshooting

| Symptom | Checks |
|---|---|
| `QAIC compiler is not executable` | Check `-DQAIC_EXECUTABLE`, file permissions, and that QAIC runs on the build host. |
| FastRPC headers or `libcdsprpc.so` not found | Run the root FastRPC build first; from `test/base_test`, verify `../../inc` and `../../src/.libs/libcdsprpc.so`, or set explicit CMake paths. |
| `libfastrpc_test_skel.so` is absent | Set executable `HEXAGON_CLANG` and `HEXAGON_LD`, choose a matching `DSP_ARCH`, and rebuild. |
| No running DSP domains found | Check remoteproc state, `/dev/fastrpc-*` nodes, daemon startup, and permissions. Use `-d ID` only after confirming the selected node exists. |
| Session open is skipped or returns `AEE_E*` | Confirm the skeleton is on the DSP search path, `DSP_LIBRARY_PATH` is set when needed, and the URI/domain matches the deployment. |
| `AEE_EUNSIGNEDMOD` or a signed-PD failure | Use the device's signing policy: `-u 1` for an allowed unsigned skeleton, or `-u 0` with the correctly signed artifact. |
| All cases are skipped | Check the target service and skeleton first; setup intentionally uses `TEST_IGNORE_MESSAGE` when the DSP service is unavailable. |
| A list command fails while execution would work | Listing uses no DSP access; check only runner arguments and metadata, not device state. |
| QAIC output is stale | Remove `builddir` and configure again, or update the source IDL and rebuild. Never edit `builddir/idl/generated/` manually. |

For daemon and device setup details, see the root [daemon documentation](../../Docs/daemons.md).

## Adding tests

### Add a case to an existing suite

1. Add the case to an existing `test_*.c` in the appropriate suite directory:
   `test/e2e/calculator/` or `test/e2e/hap_example/`.
2. Add `RUN_TEST_CASE(Group, Case)` to that group's
   `TEST_GROUP_RUNNER`.
3. Add exactly one `TEST_CASE_METADATA(Group, Case, ...)` declaration. Include
   a suite tag (`e2e`), polarity (`positive` or `negative`), and a functional
   tag. Add a domain declaration only when the case is genuinely restricted.
4. Open and close a fresh session in setup/teardown. Use
   `TEST_IGNORE_MESSAGE` when the optional DSP service is unavailable.
5. Call `REPORT_ERROR_CODE(ret)` immediately after every FastRPC API call.
6. Rebuild with `cmake --build builddir`. The existing E2E glob discovers new
   `*.c` files; no source list is required for a case in an existing suite.

Each file should define one Unity group. Cases must not share mutable session
state, and teardown must close a handle even after a body assertion fails.

### Add a new unit or feature suite

1. Create `test/unit/<suite_name>/` or `test/feature/<suite_name>/` relative to
   `test/base_test`.
2. Add `all_tests.c`, one or more `test_*.c` files, and any suite-local helper
   sources. Define the suite's group runners and per-test setup/teardown.
3. Add `<suite_name>` to `_unit_suites` in `test/unit/CMakeLists.txt` or to
   `_feat_suites` in `test/feature/CMakeLists.txt`. These lists are intentionally
   empty in this snapshot, so this step is required for a new suite.
4. Declare and call the suite entry point from `root_all_tests.c`. Unit suites
   conventionally expose `run_<suite_name>_tests()`; feature suites expose
   `run_<suite_name>_feature_tests()`.
5. Annotate every case with `TEST_CASE_METADATA` and use the established
   reporting/error-code helpers.
6. Reconfigure or let `CONFIGURE_DEPENDS` trigger a CMake regeneration, then
   run `cmake --build builddir`.

### Add a new E2E suite

1. Create `test/e2e/<suite_name>/all_tests.c` and one or more
   `test/e2e/<suite_name>/test_*.c` files. Use `test/e2e/common/e2e_session.h`
   for the standard open/close lifecycle.
2. Define `run_<suite_name>_e2e_tests()` in `all_tests.c`. CMake discovers the
   manifest and generates the central dispatcher; do not edit
   `test/e2e/CMakeLists.txt` or `root_all_tests.c` for suite registration.
3. Use the existing `fastrpc_test.idl` interface and deploy the common
   `libfastrpc_test_skel.so` when the new suite needs no new RPC methods.
4. If a method is needed, edit `idl/fastrpc_test.idl`, bump `IDL_VERSION`,
   implement it in `idl/impl/fastrpc_test_imp.c`, and let QAIC regenerate the
   stub and skeleton. Do not edit generated files.
5. Add the suite to the feature matrix and document any additional target SDK
   or device artifact.

Suite directory names must contain only lowercase letters, digits, and
underscores. Reconfigure or let `CONFIGURE_DEPENDS` trigger regeneration, then
run `cmake --build builddir`.

## Source and test conventions

- C11, `-Wall -Wextra -Werror`, and the repository's `.clang-format` apply to
  new C sources and headers.
- New source files need the BSD-3-Clause SPDX header used by the existing suite.
- Keep CMake settings target-scoped; do not add directory-wide include paths or
  compiler flags.
- Do not commit `builddir/`, generated IDL output, or compiled artifacts.
- Use `git commit -s`; contribution and review policy is in
  [CONTRIBUTING.md](../../CONTRIBUTING.md).
