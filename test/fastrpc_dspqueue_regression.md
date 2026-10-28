# FastRPC PR #399 regression driver

`fastrpc-dspqueue-regression` is a device-side diagnostic program for the
three fuzz findings addressed by PR #399:

* `--path-mode empty` and `--path-mode nomatch` exercise process initialization
  with no custom `.so` names, including the former uninitialized `lib_names`
  path from issue #397.
* `--path-mode many` creates regular files whose names contain `.so` and
  exercises custom library-name packing. The production code caps this list at
  `MAX_NON_PRELOAD_LIBS_LEN`; this is a reachability/stress check, not proof of
  the private procbuf byte-copy length from issue #396.
* repeated same-domain `dspqueue_create()`/`dspqueue_close()` rounds exercise
  the stale-domain and duplicate-join regression from issue #398.

The app is deliberately a supervisor/child program. The child performs FastRPC
initialization and queue teardown; the parent uses `waitpid(WNOHANG)` and a
monotonic deadline. A timeout terminates the child process group. It never
tries to free a queue or call `dspqueue_close()` from a watchdog thread.

## Build

Build FastRPC first, then use the normal autotools build:

```sh
./gitcompile --host=aarch64-linux-gnu
# or: ./gitcompile --host=aarch64-linux-android
make -C test fastrpc-dspqueue-regression
```

The binary is `test/.libs/fastrpc-dspqueue-regression` for a libtool build (or
`test/fastrpc-dspqueue-regression` when the platform build emits an unwrapped
binary). Deploy the binary and a DSP module matching the URI passed with
`--module-uri`. The default URI is:

```text
file:///libcalculator_skel.so?calculator_skel_handle_invoke&_modver=1.0
```

The module must be in a device DSP search directory. The process also needs
access to the matching `libcdsprpc` shared library and `/dev/fastrpc-*`.

## Device commands

Example Linux deployment:

```sh
adb push test/.libs/fastrpc-dspqueue-regression /usr/local/bin/
adb push test/v68/libcalculator_skel.so /usr/lib/rfsa/adsp/
adb shell 'LD_LIBRARY_PATH=/usr/local/lib \
  /usr/local/bin/fastrpc-dspqueue-regression --domain 3 \
  --iterations 50 --queues 2 --timeout 30 --skip-unsupported \
  --report /tmp/fastrpc-dspqueue.report'
adb pull /tmp/fastrpc-dspqueue.report .
```

Example Android deployment (paths vary by product):

```sh
adb push test/.libs/fastrpc-dspqueue-regression /data/local/tmp/
adb push test/v68/libcalculator_skel.so /data/local/tmp/
adb shell 'cd /data/local/tmp && \
  LD_LIBRARY_PATH=/data/local/tmp \
  ./fastrpc-dspqueue-regression --domain 3 --iterations 50 --queues 2 \
  --timeout 30 --skip-unsupported --report /data/local/tmp/dspqueue.report'
adb pull /data/local/tmp/dspqueue.report .
```

Without `--case`, one invocation runs the complete isolated matrix:
`normal`, `empty`, `nomatch`, `many-under`, `many-near`, `many-over`,
`queue-single`, `queue-multi`, and `queue-callback`. Each case gets a fresh
child process and its own watchdog; a failure or timeout is recorded and the
remaining cases still run. `--skip-unsupported` makes unavailable domains,
modules, or queue support produce `SKIP` instead of failing the aggregate run.
Use an outer shell watchdog as a second safety boundary when available, for
example `timeout 300` on Linux. The program prints `WATCHDOG_TIMEOUT` if its
per-case deadline expires. Capture stdout/stderr, FARF output, Android
`logcat` (or Linux `dmesg`/`journalctl`), and remoteproc/SSR/DSP crash logs.

For focused debugging, select one case without rerunning the matrix:

```sh
adb shell 'cd /data/local/tmp && ./fastrpc-dspqueue-regression \
  --case queue-callback --iterations 100 --queues 2 --timeout 30 \
  --skip-unsupported'
```

`--path-mode normal|empty|nomatch|many` remains as compatibility for focused
runs; `--case NAME` is preferred. `--near-count` and `--over-count` control the
custom-library stress sizes, while `--library-count` controls the under-limit
and focused `many` case. The default counts are bounded for a normal device
run and can be reduced further if storage is constrained.

## Interpreting results

The parent emits `CASE_START`, child operation records, `CASE_RESULT`, and one
`FINAL_SUMMARY` to stdout and, when requested, to the parent-owned report file.
A normal successful case is `PASS`. A device without the requested domain or
queue support is `SKIP` when `--skip-unsupported` is used. The intentional
`many-over` boundary is reported as `EXPECTED_BOUNDARY` whether the library
packing path returns the expected error or rejects the oversized list through
the surrounding initialization path. `WATCHDOG_TIMEOUT`, a signal termination,
sanitizer report, unexpected module-open failure, or any other unexpected
nonzero result is `FAIL`.

The driver cannot directly inspect the private `pack_proc_shared_buf_params()`
copy length, because the helper is static and process parameter packing has a
`void` public caller. It also cannot deterministically force a failed
`dspqueue_rpc_cancel_wait_signal()` RPC through the public API. Those two review
concerns remain `UNSUPPORTED` without an instrumented/fault-injection library
or a test-only seam. The external matrix is reachability/stress coverage and
must not be presented as proof of those internal branches. Queue cases close
each successfully created queue exactly once; after a partial teardown they
stop and let the isolated child exit rather than retrying cleanup.

Result status values are `PASS`, `FAIL`, `SKIP`, `TIMEOUT`,
`EXPECTED_BOUNDARY`, and `UNSUPPORTED`. The final summary is nonzero if a
required case fails or times out.
