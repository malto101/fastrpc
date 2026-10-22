# FastRPC tests

`test/` is the canonical CMake/Unity test tree. It contains `unit/`,
`feature/`, and `e2e/` suites, while the legacy Autotools launcher and its
plugin install contract remain available through `Makefile.am`.

## CMake build

Initialize the Unity submodule, build FastRPC, and configure from this
directory:

```sh
git submodule update --init
./gitcompile --host=aarch64-linux-gnu
cmake -S test -B build-test -G Ninja \
  -DFASTRPC_ROOT="$PWD" \
  -DQAIC_EXECUTABLE=/path/to/qaic
cmake --build build-test
```

The modern runner is `build-test/bin/test-fastrpc`; it intentionally does not
replace the Autotools `fastrpc_test` executable. Metadata can be listed without
a DSP device:

```sh
build-test/bin/test-fastrpc --list-tests
build-test/bin/test-fastrpc --list-groups
build-test/bin/test-fastrpc --list-tags
ctest --test-dir build-test -N
```

The default CMake project builds the base unit/feature plugin and registers the
three end-to-end suites under `test/e2e/`:

- `calculator`: sum/max and session behavior;
- `multithreading`: active parallel-sum and safe QuRT synchronization paths;
- `hap_example`: only repository-defined HAP interfaces and QuRT-based APIs.

External HAP compute-resource, memory, performance, power, cache-lock, and VTCM
interfaces are deliberately not part of this test tree. Unsupported legacy
selectors are reported as `AEE_EUNSUPPORTED` skips.

## Installed FastRPC prefix

After installing FastRPC into a prefix or staging directory, CMake can consume
the installed headers and library rather than requiring `src/.libs`:

```sh
make install DESTDIR="$PWD/stage"
cmake -S test -B build-installed -G Ninja \
  -DFASTRPC_PREFIX="$PWD/stage/usr/local" \
  -DFASTRPC_IDL_DIR="$PWD/idl" \
  -DQAIC_EXECUTABLE=/path/to/qaic
cmake --build build-installed
```

Use `FASTRPC_INCLUDE_DIR` or `FASTRPC_LIBRARY` for nonstandard `--includedir`
or `--libdir` layouts. `FASTRPC_IDL_DIR` is needed when the installed package
does not include QAIC input IDLs.

## Compatibility install layout

The legacy launcher remains `${prefix}/bin/fastrpc_test` and continues to
accept `-d`, `-U`, `-t`, and `-a`. It scans `${libdir}/fastrpc_test` for
modules exporting:

```c
int run_test(int domain_id, bool is_unsignedpd_enabled);
```

CMake installs the modern runner as `${bindir}/test-fastrpc`, the base plugin
as `${libdir}/fastrpc_test/libbase_test.so`, and source-built e2e modules with
the established names. DSP skeletons remain under
`${datadir}/fastrpc_test/v68` and `v75` when the Hexagon toolchain is available.
The existing checked-in artifacts remain available to the Autotools packaging
flow until replacement binaries have been validated on hardware.

Run the legacy launcher as before:

```sh
fastrpc_test -d 3 -U 1 -t linux -a v68
```

Host execution and the Unity metadata commands do not emulate DSP behavior;
calculator, multithreading, and supported HAP/QuRT e2e cases require a
cross-compiled AArch64 target and compatible DSP skeletons.
