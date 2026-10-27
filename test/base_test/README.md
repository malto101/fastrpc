# FastRPC `base_test`

`base_test` is a device-side AArch64 test runner for FastRPC. It calls the
QAIC-generated interface from the CPU and exercises the corresponding test
skeleton on a Hexagon DSP; it is not a host-side emulator.

## Prerequisites

- A supported AArch64 Linux or Android cross-toolchain.
- CMake 3.21 or newer and Ninja (for the standalone CMake build).
- QAIC, available as an executable file.
- Hexagon SDK tools: `hexagon-clang` and a Hexagon linker such as `ld.lld`.
- A device with FastRPC drivers, a running DSP, and access to `/dev/fastrpc-*`.
- The Unity submodule:

  ```sh
  git submodule update --init --recursive
  ```

Linux Autotools builds also require `libyaml` and `libbsd`. Android builds
require an Android NDK and `ANDROID_NDK_HOME`.

## Build for Linux

### Option 1: `./gitcompile`

From the repository root:

```sh
./gitcompile --host=aarch64-linux-gnu \
  --enable-base-test \
  --with-qaic=/path/to/qaic \
  --with-dsp-arch=73
```

This builds FastRPC and then invokes the base-test CMake build. Run
`make base-test` separately when `GITCOMPILE_NO_MAKE=1` was used.

### Option 2: CMake

Build the FastRPC library first, then configure and build the test suite:

```sh
./gitcompile --host=aarch64-linux-gnu

cmake -S test/base_test -B test/base_test/builddir -G Ninja \
  -DTARGET_PLATFORM=linux \
  -DFASTRPC_ROOT="$PWD" \
  -DFASTRPC_INCLUDE_DIR="$PWD/inc" \
  -DFASTRPC_LIBRARY="$PWD/src/.libs/libcdsprpc.so" \
  -DQAIC_EXECUTABLE=/path/to/qaic \
  -DHEXAGON_CLANG=/path/to/hexagon-clang \
  -DHEXAGON_LD=/path/to/ld.lld \
  -DDSP_ARCH=73
cmake --build test/base_test/builddir
```

## Build for Android

Set the NDK before using either option:

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk
```

### Option 1: `./gitcompile`

```sh
./gitcompile --host=aarch64-linux-android \
  --enable-base-test \
  --with-qaic=/path/to/qaic \
  --with-dsp-arch=73
```

### Option 2: CMake

Build the FastRPC library first, then configure and build the Android test
suite:

```sh
./gitcompile --host=aarch64-linux-android

cmake -S test/base_test -B test/base_test/builddir -G Ninja \
  -DTARGET_PLATFORM=android \
  -DANDROID_NDK_HOME="$ANDROID_NDK_HOME" \
  -DFASTRPC_ROOT="$PWD" \
  -DFASTRPC_INCLUDE_DIR="$PWD/inc" \
  -DFASTRPC_LIBRARY="$PWD/src/.libs/libcdsprpc.so" \
  -DQAIC_EXECUTABLE=/path/to/qaic \
  -DHEXAGON_CLANG=/path/to/hexagon-clang \
  -DHEXAGON_LD=/path/to/ld.lld \
  -DDSP_ARCH=73
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
adb push test/base_test/builddir/bin/fastrpc_test /data/local/tmp/
adb push test/base_test/builddir/bin/libfastrpc_test_skel.so /data/local/tmp/
adb push src/.libs/libcdsprpc.so /data/local/tmp/
```

Use the device's required DSP library directory for the skeleton. If
`libcdsprpc.so` is already installed, the last command is unnecessary. The
runner, FastRPC library, skeleton, DSP architecture, and target domain must
come from compatible builds.

## Running the test

Run from the repository root, or adjust the paths accordingly:

```sh
adb shell 'LD_LIBRARY_PATH=/data/local/tmp \
  DSP_LIBRARY_PATH=/data/local/tmp \
  /data/local/tmp/fastrpc_test -d 3'
```

Use `-d` to select a domain. Without `-d`, the runner discovers running DSP
domains and their `/dev/fastrpc-*` nodes.

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
