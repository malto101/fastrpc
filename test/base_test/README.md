# FastRPC `base_test`

`base_test` is a device-side AArch64 test runner for FastRPC. It calls the
QAIC-generated interface from the CPU and exercises the corresponding test
skeleton on a Hexagon DSP; it is not a host-side emulator.

## Prerequisites

- A supported AArch64 Linux or Android cross-toolchain.
- CMake 3.21 or newer and Ninja (for the standalone CMake build).
- QAIC, available as an executable file (`QAIC_EXECUTABLE` or `qaic` on `PATH`).
- A host AArch64 cross-toolchain selected with the standard `CC`, `CXX`, `AS`,
  `LD`, `AR`, `RANLIB`, and `STRIP` variables.
- A Hexagon compiler and linker for the DSP skeleton (`HEXAGON_CLANG` and
  `HEXAGON_LD`, or `hexagon-clang` and `ld.lld` on `PATH`). A host GCC does not
  replace the Hexagon compiler for the DSP-side library.
- A device with FastRPC drivers, a running DSP, and access to `/dev/fastrpc-*`.
- The Unity submodule:

  From the workspace root:

  ```sh
  git submodule update --init --recursive
  ```

Linux Autotools builds also require `libyaml` and `libbsd`. Android builds
require an Android NDK and `ANDROID_NDK_HOME`.

The CMake platform files reuse the same cross-tool variables as the
FastRPC library. For Linux, use the toolchain setup from the root build:

```sh
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
export AS=aarch64-linux-gnu-as
export LD=aarch64-linux-gnu-ld
export AR=aarch64-linux-gnu-ar
export RANLIB=aarch64-linux-gnu-ranlib
export STRIP=aarch64-linux-gnu-strip
```

For Android, use the NDK tools in the same way:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk
export PATH="$PATH:$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
export CC=aarch64-linux-android34-clang
export CXX=aarch64-linux-android34-clang++
export AS=llvm-as
export LD=ld
export AR=llvm-ar
export RANLIB=llvm-ranlib
export STRIP=llvm-strip
```

Set the DSP-only tools explicitly when they are not on `PATH`:

```sh
export QAIC_EXECUTABLE=/path/to/qaic
export HEXAGON_CLANG=/path/to/hexagon-clang
export HEXAGON_LD=/path/to/ld.lld
# export DSP_ARCH=73  # optional; 73 is the default
```

For a standalone Android CMake build, also select Android:

```sh
export TARGET_PLATFORM=android
```

Explicit `-D...` CMake values and `--with-qaic`/`--with-dsp-arch` Autotools
options take precedence over environment variables. `CMAKE_C_COMPILER`,
`CMAKE_CXX_COMPILER`, `CMAKE_AR`, `CMAKE_RANLIB`, `CMAKE_LINKER`, and
`CMAKE_STRIP` may be supplied explicitly when a CMake cache value is preferred.

## Build for Linux

### Option 1: `./gitcompile`

From the repository root:

```sh
./gitcompile --host=aarch64-linux-gnu \
  --enable-base-test
```

This builds FastRPC and then invokes the base-test CMake build. Run
`make base-test` separately when `GITCOMPILE_NO_MAKE=1` was used.

### Option 2: CMake

Build the FastRPC library first, then configure and build the test suite:

```sh
./gitcompile --host=aarch64-linux-gnu

cmake -S test/base_test -B test/base_test/builddir -G Ninja
cmake --build test/base_test/builddir
```

## Build for Android

Set the target platform before using either option. The Android tool
variables and `ANDROID_NDK_HOME` should already be exported as shown above:

```sh
export TARGET_PLATFORM=android
```

### Option 1: `./gitcompile`

```sh
./gitcompile --host=aarch64-linux-android \
  --enable-base-test
```

### Option 2: CMake

Build the FastRPC library first, then configure and build the Android test
suite:

```sh
./gitcompile --host=aarch64-linux-android

cmake -S test/base_test -B test/base_test/builddir -G Ninja
cmake --build test/base_test/builddir
```

## Expected artifacts

After a successful build:

```text
test/base_test/builddir/bin/fastrpc_test
test/base_test/builddir/bin/libfastrpc_test_skel.so
```

`fastrpc_test` is the AArch64 CPU-side runner. `libfastrpc_test_skel.so` is
the Hexagon DSP-side skeleton. QAIC-generated sources are placed under
`test/base_test/builddir/idl/generated/`.

## Deployment

The following example deploys to an Android device over `adb`:

```sh
adb push test/base_test/builddir/bin/fastrpc_test /usr/local/bin/
adb push test/base_test/builddir/bin/libfastrpc_test_skel.so /usr/local/share/
```

Use the device's required DSP library directory for the skeleton. If
`libcdsprpc.so` is already installed, the last command is unnecessary. The
runner, FastRPC library, skeleton, DSP architecture, and target domain must
come from compatible builds.

## Running the test

Run from the repository root, or adjust the paths accordingly:

```sh
adb shell DSP_LIBRARY_PATH=/usr/local/share \
  /usr/local/bin/fastrpc_test -d 3
```

Use `-d` to select a domain. Without `-d`, the runner runs on all available DSP nodes.

### Sample output

```text
BEGIN DOMAIN
Name: cdsp
ID: 3

  TEST CalculatorE2E.SumAndMaximum
    Result: PASS

DOMAIN SUMMARY
Name: cdsp
ID: 3
Tests: 1
Passed: 1
Failed: 0
Skipped: 0
Result: PASS

END DOMAIN
============================================================

OVERALL SUMMARY
Tests: 1
Passed: 1
Failed: 0
Skipped: 0
Result: PASS
```

## CLI reference

```text
fastrpc_test [options]
```

| Option | Description |
|---|---|
| `-d DOMAIN` | Select a DSP domain; repeat to select multiple domains. |
| `-u 0\|1` | Disable or enable unsigned-PD control (default: `1`). |
| `-l tests\|groups\|tags` | List tests, groups, or tags and exit. |
| `--list-tests` | List test cases and exit. |
| `--list-groups` | List test groups and exit. |
| `--list-tags` | List registered tags and exit. |
| `--any-tags TAG` | Select a test matching at least one supplied tag; repeatable. |
| `--all-tags TAG` | Select a test matching every supplied tag; repeatable. |
| `-g GROUP` / `-G TEXT` | Select an exact group, or groups containing `TEXT`. |
| `-n CASE` / `-N TEXT` | Select an exact case, or cases containing `TEXT`. |
| `-r COUNT` | Repeat selected tests `COUNT` times. |
| `-v` | Show passing-test output. |
| `-s`, `--silent` | Hide passing-test output. |
| `-h`, `--help` | Print help and exit. |

Examples:

```sh
# Run CDSP.
./test/base_test/builddir/bin/fastrpc_test -d 3

# Run only the named case twice.
./test/base_test/builddir/bin/fastrpc_test \
  -d 3 -g CalculatorE2E -n SumAndMaximum -r 2

# Inspect test metadata without opening a DSP session.
./test/base_test/builddir/bin/fastrpc_test --list-tests
```
