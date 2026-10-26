# base_test

Feature test suite for the Qualcomm FastRPC library.

## Process isolation and concurrency

By default, each selected test case runs in a fresh worker process created by
`fork()` followed immediately by `exec()` of the current `fastrpc_test` binary.
The parent test runner owns Unity counters and reporting; the worker owns its own
FastRPC session and is never entered through `fork()` after FastRPC initialization.
The worker sends a bounded, versioned result frame and captured output to the parent.
The supervisor also uses a close-on-exec handshake to distinguish `exec()`/descriptor
setup failures from malformed worker results. Expected signals are classified only
from `waitpid()` status; a malformed or trailing result frame is never reclassified
as a passing signal. Signals, non-zero exits, malformed results, and timeouts fail
only that case and later cases continue to run.

Only `fastrpc_test` is required for the host-side process-isolation path. The
worker mode is selected through private command-line arguments and is not a
separate deployment artifact. `FASTRPC_TEST_WORKER` may be used to override the
executable for an external diagnostic worker. Use `--timeout-ms <milliseconds>`
to bound a case. `--isolation=in-process` is a debugging escape hatch only and
does not protect the runner from a crash.

Tests that intentionally exercise a signal must declare
`TEST_METADATA_EXPECTED_SIGNAL()` in the metadata declaration. A passing
expected-signal policy requires the worker to terminate with that signal; normal
return or a different signal is a failure. Tags do not alter signal semantics.
Worker threads and helper processes must return status through caller-owned
contexts; they must not call Unity assertions or mutate output/reporting globals.
Use the thread-group helper for future tests so partial creation and start-gate
cleanup remain consistent.

### Test metadata and domain support

`TEST_CASE_METADATA(group, name, ...)` is the preferred way to describe a test.
It is a single, constructor-registered descriptor for tags, supported domains,
and expected-signal behavior:

```c
TEST_CASE_METADATA(RemoteE2E, UsesCdspOnly,
                   TEST_METADATA_TAGS("e2e", "positive"),
                   TEST_METADATA_DOMAINS(CDSP_DOMAIN_ID));
```

`TEST_METADATA_TAGS()` accepts string literals, `TEST_METADATA_DOMAINS()` accepts
numeric FastRPC domain IDs, and `TEST_METADATA_EXPECTED_SIGNAL()` accepts a
signal number plus `TEST_SIGNAL_PASS` or `TEST_SIGNAL_FAIL`. Metadata declarations
must be at file scope; their values and arrays have static storage duration. Each
supplied tag/domain list must be nonempty. A test with no metadata, or a descriptor
without `TEST_METADATA_DOMAINS()`, supports every known domain.

Domain support is evaluated by the parent runner before setup/teardown and before
an isolated worker is launched. A test whose allowlist excludes the selected
domain is counted as ignored without opening a DSP session or starting a worker.
With multiple `-d` selections, the decision is made independently for each
selected domain. Every `(group, name)` must have exactly one metadata descriptor;
a second descriptor is rejected even when all fields are identical. Invalid tag,
domain, or duplicate declarations are reported before test execution.

During normal execution, the parent records each selected Unity dispatch and
checks it against the metadata registry after `UnityMain()` completes. A selected
Unity case without metadata, or a selected metadata entry that is never dispatched,
is an execution failure. This check is intentionally skipped by list-only modes,
which remain metadata-only and do not discover DSP domains or launch workers.
`--list-tags` lists all registered tags rather than only tags used by selected tests.
`IGNORE_TEST()` dispatches are included in the same completeness check.

Every metadata-bearing test must use `TEST_CASE_METADATA()`; signal policy is
expressed with `TEST_METADATA_EXPECTED_SIGNAL()` in that descriptor.

Worker threads and helper processes must return status through caller-owned
contexts; they must not call Unity assertions or mutate output/reporting globals.
Use the thread-group helper for future tests so partial creation and start-gate
cleanup remain consistent.

The metadata listing commands (`--list-tests`, `--list-groups`, and `--list-tags`)
read the same registry and do not perform DSP discovery.

Tests run on the **host CPU** and exercise the DSP over FastRPC. The build
produces a standalone host test binary (`fastrpc_test`) and a Hexagon DSP shared
library (`libfastrpc_test_skel.so`). Deploy both artifacts for device execution.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Step 1 — Build the fastrpc library](#step-1--build-the-fastrpc-library)
3. [Step 2 — Configure QAIC generation](#step-2--configure-qaic-generation)
4. [Step 3 — Build with CMake](#step-3--build-with-cmake)
5. [Build artifacts](#build-artifacts)
6. [Running the tests](#running-the-tests)
7. [Adding tests](#adding-tests)
8. [Contributing — rules and policies](#contributing--rules-and-policies)
   - [Linux kernel coding style](#linux-kernel-coding-style)

---

## Prerequisites

Calculator and HAP use the common `idl/fastrpc_test.idl` interface and
`idl/impl/fastrpc_test_imp.c` implementation. Both E2E suites share the normal
base-test domain, unsigned-PD, reporting, and generated-stub infrastructure.
Deploy `libfastrpc_test_skel.so` for these cases.

The calculator cases cover sum, maximum, and invalid input handling. The HAP
case covers FARF runtime logging configuration. Both suites are tagged `e2e`.

A target-side HAP SDK is required to build and run the FARF implementation.

---

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | ≥ 3.21 | Build system |
| Ninja | any | Recommended generator |
| clang / clang++ | any recent | Cross-compiler for aarch64 |
| llvm-ar, llvm-ranlib, ld.lld, llvm-strip | matching clang | LLVM toolchain utilities |
| QAIC | Hexagon SDK | IDL stub/skel code generation |
| Android NDK | r25c or later | Android target only |

From the fastrpc repository root, initialise the Unity submodule before the
first build:

```bash
git submodule update --init --recursive
```

---

## Step 1 — Build the fastrpc library

`base_test` links against `libcdsprpc.so` from the fastrpc repository root.
Build it first:

```bash
# Linux (aarch64)
cd /path/to/fastrpc
./gitcompile --host=aarch64-linux-gnu

# Android (aarch64)
./gitcompile --host=aarch64-linux-android
```

The CMake project accepts either an in-tree build or an installed/system
FastRPC library. Use `FASTRPC_ROOT` for the source checkout, or set
`FASTRPC_PREFIX`, `FASTRPC_INCLUDE_DIR`, and `FASTRPC_LIBRARY` explicitly.
The normal in-tree output is `src/.libs/libcdsprpc.so`, including compatible
versioned filenames.

For cross builds, set the compiler and related tools in the environment as
described by the FastRPC build instructions; the test build does not override
the toolchain's dynamic linker.

```bash
# Optional explicit installed/system library configuration
-DFASTRPC_PREFIX=/path/to/prefix
-DFASTRPC_INCLUDE_DIR=/path/to/prefix/include/fastrpc
-DFASTRPC_LIBRARY=/path/to/prefix/lib/libcdsprpc.so
```

---

## Step 2 — Configure QAIC generation

The CMake build runs **QAIC** automatically to generate `fastrpc_test.h`, the
CPU stub, and the DSP skel from `idl/fastrpc_test.idl`. Pass the compiler path
when configuring. QAIC resolves shared IDL includes from `<FASTRPC_ROOT>/idl`.

```bash
-DQAIC_EXECUTABLE=/path/to/qaic
```

Generated files are written under `<builddir>/idl/generated/` and refreshed
when the source IDL, its included IDLs, or the QAIC executable changes.

The three generated files serve distinct roles:

| File | Compiled into | Purpose |
|------|--------------|---------|
| `fastrpc_test.h` | both | Shared API declarations |
| `fastrpc_test_stub.c` | `fastrpc_test` (host) | Marshals CPU→DSP calls over FastRPC |
| `fastrpc_test_skel.c` | `libfastrpc_test_skel.so` (DSP) | Dispatches incoming RPC to `fastrpc_test_imp.c` |

---

## Step 3 — Build with CMake

### Linux — aarch64-linux-gnu

```bash
cmake -B builddir \
      -DFASTRPC_ROOT=/path/to/fastrpc \
      -DQAIC_EXECUTABLE=/path/to/qaic \
      -G Ninja

cmake --build builddir
```

### Android — aarch64-linux-android (API 35)

```bash
cmake -B builddir \
      -DTARGET_PLATFORM=android \
      -DFASTRPC_ROOT=/path/to/fastrpc \
      -DQAIC_EXECUTABLE=/path/to/qaic \
      -DANDROID_NDK_HOME=/path/to/android-ndk \
      -G Ninja

cmake --build builddir
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `TARGET_PLATFORM` | `linux` | Target platform: `linux` or `android` |
| `FASTRPC_ROOT` | auto-detected | Path to the fastrpc repository root |
| `FASTRPC_PREFIX` | — | FastRPC installation prefix |
| `FASTRPC_INCLUDE_DIR` | derived | FastRPC headers override |
| `FASTRPC_LIBRARY` | derived | `libcdsprpc` shared-library override |
| `QAIC_EXECUTABLE` | — | Path to the QAIC compiler (required) |
| `ANDROID_NDK_HOME` | — | Path to the Android NDK root (android only) |
| `DSP_ARCH` | `73` | Generic base-test DSP architecture (e.g. `68`, `73`, `75`) |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |

The calculator and HAP cases use the normal `libfastrpc_test_skel.so` artifact.
The HAP implementation requires the target HAP SDK headers when building the
common DSP implementation.

`FASTRPC_ROOT` is auto-detected as `../..` relative to this directory when not
set explicitly, which is correct when `base_test` lives at
`<fastrpc_root>/test/base_test`.

---

## Build artifacts

All artifacts are placed in `<builddir>/bin/`.

### `fastrpc_test` — host test binary

The CPU-side test executable.  Links together:

- `root_all_tests.c` — `main()`, initialises reporting, dispatches suite runners
- suite static libraries containing the registered test sources
- `fastrpc_test_stub` — QAIC-generated CPU→DSP RPC stub
- `utils` — reporting and FastRPC helper utilities
- `unity` — ThrowTheSwitch/Unity test framework (fixture + memory extensions)
- `libcdsprpc.so` — FastRPC runtime (dynamically linked)

Push to the target device and run directly:

```bash
adb push builddir/bin/fastrpc_test /data/local/tmp/
adb shell /data/local/tmp/fastrpc_test
```

### `libfastrpc_test_skel.so` — Hexagon DSP shared library

The DSP-side skel, compiled with `--target=hexagon -mv<DSP_ARCH>`.  Built
automatically when `clang` is found in `PATH`; skipped silently otherwise
(the host binary is unaffected).

Contains:

- `fastrpc_test_skel.c` — QAIC-generated RPC dispatch table
- `fastrpc_test_imp.c` — DSP-side method implementations

Push to the DSP library search path before running the host binary:

```bash
adb push builddir/bin/libfastrpc_test_skel.so /usr/lib/rfsa/adsp/
# or set DSP_LIBRARY_PATH at runtime:
adb shell "DSP_LIBRARY_PATH=/data/local/tmp /data/local/tmp/fastrpc_test"
```

---

## Running the tests

```bash
# Run all tests on every discovered, running FastRPC DSP domain
./bin/fastrpc_test

# List test metadata without accessing a DSP or running tests
./bin/fastrpc_test -l tests
./bin/fastrpc_test -l groups
./bin/fastrpc_test -l tags

# Equivalent long forms
./bin/fastrpc_test --list-tests
./bin/fastrpc_test --list-groups
./bin/fastrpc_test --list-tags

# Verbose output (prints each test name as it runs)
./bin/fastrpc_test -v

# Target a specific DSP domain
./bin/fastrpc_test -d 3

# Target only a selected set of DSP domains (repeat -d)
./bin/fastrpc_test -d 0 -d 3 -d 4

# Disable unsigned PD (for signed skels)
./bin/fastrpc_test -u 0

# Filter by tag (OR — run tests that have any of these tags)
./bin/fastrpc_test --any-tags feature --any-tags positive

# Filter by tag (AND — run tests that have all of these tags)
./bin/fastrpc_test --all-tags Remote --all-tags positive

# Suppress output for passing tests — only failures and skips are shown
./bin/fastrpc_test --silent

# Repeat every selected case; counts are aggregated across repeats
./bin/fastrpc_test -r 2

# E2E tests only (all E2E cases carry the `e2e` tag)
./bin/fastrpc_test --all-tags e2e -d 3
```

Listing commands print one sorted entry per line. Existing group, test-name,
and tag filters narrow test and group listings. Tag listings always show every
available tag. Test setup and DSP discovery are never performed.

FastRPC keeps `-d <domain>` for domain selection. `-r N` is handled by the
project runner and aggregates counts across all repeats; `-r 0` is rejected as
an invalid run.

Infrastructure regressions for the process supervisor and thread helper are
validated with disposable local fixtures under `/tmp`; they are not part of the
deployed FastRPC test application or this CMake project. The fixtures must be
removed after verification and are intentionally not installed or committed.

The base-test executable itself is a cross-compiled device test and is not a
host-side FastRPC emulator.

`--silent` buffers each test's header/body/footer output and only prints it
if the test fails or is skipped; passing tests produce no output at all. The
final summary is unaffected. Because the buffer is only flushed after a test
completes, a hang inside a test won't show anything on the console until it
finishes or is interrupted — unlike the default mode, where the header box
prints live before the test body runs.

---

## Adding tests

### Add a new test case to an existing group

1. Add a `TEST(<GroupName>, <CaseName>) { ... }` block to the suite source.
2. Register it in the suite's `TEST_GROUP_RUNNER(<GroupName>)` with
   `RUN_TEST_CASE(<GroupName>, <CaseName>)`.
3. Annotate it with `TEST_CASE_METADATA()` using the suite's tag vocabulary.
4. Rebuild — no CMake changes are needed.

Each test source should contain one group, its per-test setup/teardown functions,
and its group runner. There is no once-per-group setup or teardown. The
`CONFIGURE_DEPENDS` glob picks up new `test_*.c` files.

#### Add a new test group

1. Create `test/feature/<suite>/test_<name>.c` or
   `test/unit/<suite>/test_<name>.c`.
2. Define the group, per-test setup/teardown, test cases, and group runner.
3. Add the group runner to the suite's `all_tests.c`.
4. Rebuild.

### Add a new feature suite

1. Create `test/feature/<suite_name>/` containing:
   - `all_tests.c` — defines `void run_<suite_name>_feature_tests(void)`
   - One or more `test_*.c` files
   - Any utility `*.c` / `*.h` files needed by the suite
2. In `root_all_tests.c`:
   - Add `void run_<suite_name>_feature_tests(void);` (no `extern` — see
     [Linux kernel coding style](#linux-kernel-coding-style))
   - Call it inside `run_all_tests()`
3. In `test/feature/CMakeLists.txt`:
   - Append `<suite_name>` to the `_feat_suites` list
4. Rebuild.

### Add a new IDL method

1. Edit `idl/fastrpc_test.idl` — add the method declaration.
2. Regenerate with QAIC (see [Step 2](#step-2--configure-qaic-generation)).
3. Implement the method in `idl/impl/fastrpc_test_imp.c`.
4. Rebuild.

---

## Contributing — rules and policies

### Code style

- **Language standard:** C11 (`-std=c11`).  No compiler extensions
  (`CMAKE_C_EXTENSIONS OFF`).
- **Warnings are errors:** `-Wall -Wextra -Werror`.  Every new file must
  compile cleanly with no warnings.
- **Formatting is enforced by `.clang-format`.**  Run `clang-format -i` on
  every changed `.c`/`.h` file before committing; CI treats a non-conforming
  file as a build failure.  Key points from the config:
  - **Indentation:** 4 spaces.  No tabs (`UseTab: Never`).
  - **Line length:** 100 columns hard limit (`ColumnLimit: 100`).
  - **Braces:** Linux style — the opening brace goes on its own line for
    function bodies, but on the same line as `if`/`for`/`while`/`switch`/
    `struct`/`enum`.  Single-statement bodies without braces are allowed
    (`if (!c) return AEE_EBADPARM;`-style is reformatted to a line break,
    not collapsed onto one line).
  - **Pointers:** right-aligned (`int *result`, not `int* result` or
    `int * result`).
  - **`switch` statements:** every `case` gets its own line and its own
    `break;` line — no collapsing multiple cases or case+break onto a
    single line.
  - **`#include` ordering:** within each block, includes are sorted
    case-sensitively; local (`"..."`) headers before system (`<...>`)
    headers, each block separated by a blank line.
  - **No manual alignment** of trailing `=`, trailing comments, or struct
    members — clang-format decides spacing; don't hand-pad columns.
- **Naming:**
  - Functions and variables: `lower_snake_case`
  - Macros and constants: `UPPER_SNAKE_CASE`
  - Test groups: `UpperCamelCase` (Unity convention)
  - Test cases: `UpperCamelCase` (Unity convention)

### Linux kernel coding style

`base_test` follows the
[Linux kernel coding style](https://docs.kernel.org/process/coding-style.html),
with three deliberate exceptions:

1. **Indentation:** 4 spaces, no tabs (kernel uses 8-column tabs) — enforced
   by `.clang-format` (`IndentWidth: 4`, `UseTab: Never`).
2. **Line length:** 100 columns (kernel prefers 80) — enforced by
   `.clang-format` (`ColumnLimit: 100`).
3. **Comments:** existing Doxygen-style tags (`@brief` / `@param` / `@return`
   / `@file`) are retained rather than converted to kernel-doc format.
4. **Struct/enum typedefs:** existing `typedef struct { ... } foo_t;` /
   `typedef enum { ... } foo_t;` usage is retained as-is; new code may
   continue to use this pattern rather than bare `struct foo` / `enum foo`.

Everything else in the kernel doc applies as written:

- **No `extern` keyword on function prototypes** — only on variables that
  need external linkage.
- **One declaration per line** — no multiple statements or multiple
  assignments/declarations packed onto a single line.
- **`goto` is permitted** for functions with multiple exit paths needing
  common cleanup, using descriptive labels (`out_free_buffer:`,
  `err_free_foo:`), never generic `err1:` / `err2:`.
- **Brace placement, spacing, pointer alignment, and `switch`/`case`
  formatting** are enforced via `.clang-format` (`BreakBeforeBraces: Linux`,
  `PointerAlignment: Right`, `AllowShortCaseLabelsOnASingleLine: false`) —
  see [Code style](#code-style) above.
- **Naming stays `lower_snake_case`** for functions/variables (no camelCase,
  no Hungarian notation) — already a documented rule above, now also
  inherited from kernel style.

### File headers

Every new `.c` and `.h` file must begin with the SPDX licence header:

```c
// Copyright (c), Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
```

Every new `CMakeLists.txt` must begin with:

```cmake
# Copyright (c), Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
```

### Test design rules

- **One `TEST_GROUP` per file.**  Each `test_*.c` file defines exactly one
  `TEST_GROUP` and its `TEST_GROUP_RUNNER`.
- **Independent tests.**  Every test must be able to run in isolation.
  `TEST_SETUP` opens a fresh DSP session; `TEST_TEAR_DOWN` closes it
  unconditionally.  Tests must not share mutable state across cases.
- **Always call `REPORT_ERROR_CODE(ret)`** immediately after any FastRPC API
  call.  This records the return value for the consolidated test footer.
- **Use `TEST_IGNORE_MESSAGE`** (not `TEST_FAIL`) when a DSP session cannot be
  opened.  Hardware absence is not a test failure.
- **Annotate every test case** with `TEST_CASE_METADATA` so tag-based filtering
  works. Minimum required tags: feature classification (e.g. `"feature"`),
  polarity (`"positive"` or
  `"negative"`), and a functional tag such as `"Remote"` or `"RpcMem"`.
- **No `printf` in `TEST_SETUP` / `TEST_TEAR_DOWN`** beyond the session open/
  close lines already established.  Diagnostic output belongs in the test body.
- **Elapsed time must be reported** for any test that calls a timed DSP method.
  Use the `PRIu64` format specifier for `uint64_t` values.

### IDL rules

- **Do not edit `idl/generated/` by hand.**  All changes must go through
  `fastrpc_test.idl` and be regenerated with QAIC.
- **Bump `IDL_VERSION`** in `fastrpc_test.idl` whenever a method is added,
  removed, or its signature changes.  The skel performs a version check at
  `open()` time and returns `AEE_ESTUBSKELVERMISMATCH` on mismatch.
- DSP implementations should use only the APIs required by the interface and
  the target SDK used by the selected sample.

### CMake rules

- **Do not add `include_directories()` or `add_compile_options()` at the
  directory level.**  Use `target_*` commands exclusively so that flags are
  scoped to the targets that need them.
- **New suites** are registered by appending to `_feat_suites` in
  `test/feature/CMakeLists.txt`.  No other CMake file needs to change.
- **`CONFIGURE_DEPENDS`** is set on all `file(GLOB ...)` calls.  Adding or
  removing a `test_*.c` file triggers an automatic CMake re-run on the next
  build; no manual CMake invocation is needed.

### Git hygiene

- **Never commit build artefacts** (`builddir/`, `*.o`, `*.so`, `*.a`).
- **Never commit the generated IDL files** if they were regenerated solely to
  reformat whitespace.  Only commit `idl/generated/` changes that result from
  a real IDL version bump.
- **One logical change per commit.**  Separate test additions from
  infrastructure changes.
- **Commit message format:**

  ```
  component: short imperative summary (≤ 72 chars)

  Optional body explaining why, not what.  Wrap at 72 columns.
  ```

  Examples:
  ```
  remote: add session lifecycle coverage
  idl: bump IDL_VERSION to 1.2.5, add malloc_free_stress method
  utils: remove csv reporting from base_test
  ```
