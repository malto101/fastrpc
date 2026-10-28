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
  /usr/local/bin/fastrpc-dspqueue-regression --path-mode normal \
  --domain 3 --iterations 50 --queues 2 --timeout 30'
```

Example Android deployment (paths vary by product):

```sh
adb push test/.libs/fastrpc-dspqueue-regression /data/local/tmp/
adb push test/v68/libcalculator_skel.so /data/local/tmp/
adb shell 'cd /data/local/tmp && \
  LD_LIBRARY_PATH=/data/local/tmp \
  ./fastrpc-dspqueue-regression --path-mode normal \
  --domain 3 --iterations 50 --queues 2 --timeout 30'
```

Run the empty and no-match cases in separate processes, before any other
FastRPC test in that process:

```sh
adb shell 'cd /data/local/tmp && ./fastrpc-dspqueue-regression \
  --path-mode empty --iterations 1 --queues 1 --timeout 30'
adb shell 'cd /data/local/tmp && ./fastrpc-dspqueue-regression \
  --path-mode nomatch --iterations 1 --queues 1 --timeout 30'
```

Stress the custom-name path with a controlled number of names:

```sh
adb shell 'cd /data/local/tmp && ./fastrpc-dspqueue-regression \
  --path-mode many --library-count 16 --iterations 1 --timeout 30'
```

Use an outer shell watchdog as a second safety boundary when available, for
example `timeout 45` on Linux. The program prints `WATCHDOG_TIMEOUT` if its
own deadline expires. Capture stdout/stderr, FARF output, Android `logcat` (or
Linux `dmesg`/`journalctl`), and any remoteproc/SSR/DSP crash logs.

## Interpreting results

`CHILD_RESULT PASS` means the selected module opened, every queue creation and
close returned success, and the child exited before the deadline. A device
without the requested domain or queue support returns exit status 77 and is a
**SKIP**, not a pass. `WATCHDOG_TIMEOUT`, a signal termination, sanitizer
report, or unexpected nonzero result is a failure.

The driver cannot directly inspect the private `pack_proc_shared_buf_params()`
copy length, because the helper is static and process parameter packing has a
`void` public caller. It also cannot deterministically force a failed
`dspqueue_rpc_cancel_wait_signal()` RPC through the public API. Conclusive tests
for those two review concerns require an instrumented/fault-injection library
or a test-only seam. The normal cases here are external smoke/stress tests and
must not be presented as proof of those internal branches.
