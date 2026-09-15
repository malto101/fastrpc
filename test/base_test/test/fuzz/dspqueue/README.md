# Fuzz harness: `dspqueue_create()` parameter validation

Complements (does not replace) the `DspQueueCreate` unit tests in
`test/base_test/test/unit/dspqueue/`. Those tests assert specific
`AEE_*` error codes for a handful of hand-picked bad inputs; this harness
explores the same parameter space via coverage-guided fuzzing to find
inputs nobody thought to write down.

## Scope

Only `dspqueue_create()`. Its `queue != NULL`, domain-range, `flags == 0`,
and queue-size checks all run in CPU code before any `rpcmem_alloc`/
`fastrpc_mmap`/DSP interaction (see `src/dspqueue/dspqueue_cpu.c`), so this
is reachable and meaningful without a live DSP device.

`dspqueue_write`/`read`/`peek`/`get_stat`/`close`/`export` are **not**
covered here: they take an opaque `dspqueue_t` backed by a `struct
dspqueue` private to `dspqueue_cpu.c`, and several dereference it
immediately with no upfront NULL check. Fuzzing them meaningfully needs
either a real DSP-created queue or a test-only fake-queue constructor
added inside `dspqueue_cpu.c` — deferred as future work pending that
design decision.

## Build

Fuzz suites are opt-in — off by default, since they need a clang
toolchain with the libFuzzer/sanitizer runtime for the target:

```sh
cmake -B builddir \
  -DENABLE_FUZZ_TESTS=ON \
  -DFASTRPC_ROOT=/path/to/fastrpc \
  -DQAIC_EXECUTABLE=/path/to/qaic \
  -G Ninja
cmake --build builddir
```

This compiles this suite into `builddir/bin/test-fastrpc` itself (cross-compiled
the same way as the rest of `test-fastrpc`) rather than producing a separate
binary — see the root `CMakeLists.txt` "FUZZ TESTS" section. `root_all_tests.c`
owns `main()` and hands argv to libFuzzer's driver via `LLVMFuzzerRunDriver()`
when it sees `--fuzz`, instead of libFuzzer supplying its own `main()`.

## Run

`--fuzz` runs every registered suite by default (currently just this one),
each against its own checked-in corpus, unless you narrow or override it:

```sh
adb push builddir/bin/test-fastrpc /data/local/tmp/
adb shell 'cd /data/local/tmp && ./test-fastrpc --fuzz -max_len=32'
```

The seed files under `corpus/dspqueue_create/` are embedded into the binary
at build time (see `test/fuzz/cmake/embed_corpus.cmake`) — if the default
corpus dir (`corpus/dspqueue_create`, resolved relative to the *current
working directory at run time*) doesn't already exist, `root_all_tests.c`
writes those embedded seeds out to it before handing the path to libFuzzer,
so pushing `test/fuzz/dspqueue/corpus/` separately is no longer required to
get started. This only covers the small checked-in seed set, though — once
you're running real coverage-guided sessions and want libFuzzer to keep
growing the corpus across runs, `adb push`/`adb pull` a real corpus
directory as usual; the auto-materialization step only fires when the
directory is missing, and never touches one that already exists.

Once the run finishes, `root_all_tests.c` removes the corpus directory again
if (and only if) this same process is the one that auto-materialized it —
so a device that never had `corpus/dspqueue_create/` pushed to it is left
exactly as it started, instead of accumulating a growing corpus (including
any new coverage-increasing inputs libFuzzer found) run over run. A corpus
directory you pushed or pointed at explicitly is never touched or removed.

Narrow to one suite by its registered tag — the same `--tags`/`--any-tags`/
`--all-tags` flags used to filter Unity test cases (see `test_utils.h`) also
select which fuzz suite(s) run:

```sh
adb shell 'cd /data/local/tmp && ./test-fastrpc --fuzz --tags dspqueue_create -max_len=32'
```

Pass an explicit corpus path to override the default; only valid once tags
have narrowed the selection to exactly one suite:

```sh
adb shell /data/local/tmp/test-fastrpc --fuzz --tags dspqueue_create -max_len=32 /data/local/tmp/some/other/corpus
```

Without a live DSP session, every call is expected to fail cleanly
(`AEE_EBADPARM`/`AEE_ERPC`/`AEE_ENOTINITIALIZED`, etc.) — a crash or
ASan/UBSan abort is a real bug.

## Corpus byte layout

Seeds live under `corpus/dspqueue_create/`, not directly under `corpus/` —
this dir covers the whole `dspqueue` module, and a future harness for another
`dspqueue_*` API would need its own seed format and thus its own
`corpus/<api>/` subdir alongside this one.

`fuzz_dspqueue_create_run` interprets each input as a raw, little-endian
memcpy of `struct fuzz_input` (see `fuzz_dspqueue_create.c`) — no
length-prefixing or parsing, so seeds must match this layout exactly.
Inputs shorter than 20 bytes are rejected outright.

| Offset | Size | Field             | Notes                                                                 |
|-------:|-----:|-------------------|------------------------------------------------------------------------|
| 0      | 4    | `domain`          | `int32_t`, LE. e.g. `-1` = current process domain, `99` = out-of-range |
| 4      | 4    | `flags`           | `uint32_t`, LE. Only `0` passes validation                            |
| 8      | 4    | `req_queue_size`  | `uint32_t`, LE. `0` = use default; `16777216` (`DSPQUEUE_MAX_QUEUE_SIZE`) is the accept/reject boundary |
| 12     | 4    | `resp_queue_size` | `uint32_t`, LE. Same boundary as `req_queue_size`                      |
| 16     | 1    | `null_queue_ptr`  | `uint8_t`. Only bit 0 is read: `1` = pass `NULL` as the queue out-pointer |
| 17     | 3    | *(padding)*       | Struct padding to the 4-byte alignment of the `int32_t`/`uint32_t` fields. Content is never read, but the bytes must be present — total size is `sizeof(struct fuzz_input)` == 20 on both x86_64 and aarch64 |

If the struct's fields ever change, regenerate seeds to match rather than
hand-editing the existing binaries, e.g.:

```python
import struct
# domain, flags, req_queue_size, resp_queue_size, null_queue_ptr, padding
data = struct.pack('<iIIIB3x', 0, 0, 4096, 4096, 0)
open('corpus/dspqueue_create/valid_small_queue', 'wb').write(data)
```
