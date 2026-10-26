# FastRPC tests

This directory contains two different test runners. Choose the one that
matches the workflow you need.

| Runner | Location | Build | Purpose |
|---|---|---|---|
| Legacy plugin runner | `test/fastrpc_test.c` | Normal Autotools build | Loads the prebuilt sample shared libraries and runs their `run_test` entry points. |
| Base-test runner | `test/base_test/` | Opt-in CMake build | Runs the Unity-based AArch64 host tests and calls the generated FastRPC DSP skeleton. |

The base-test runner is the maintained path for adding CMake/Unity test cases.
It is not a host emulator: it must run on an AArch64 target with FastRPC
device and DSP support. See [`base_test/README.md`](base_test/README.md) for
AArch64 build, deployment, CLI, domain, troubleshooting, and add-test
instructions.

## Legacy plugin runner

The legacy executable loads test plugins from its configured library directory.
A typical installed invocation is:

```bash
fastrpc_test -d 3 -U 1 -t linux -a v68
```

The legacy options are:

- `-d DOMAIN` — domain ID: `0` ADSP, `1` MDSP, `2` SDSP, `3` CDSP,
  `4` CDSP1, `5` GDSP0, or `6` GDSP1. The default is `3` on targets with CDSP.
- `-U 0|1` — use a signed (`0`) or unsigned (`1`) protection domain. The
  default is `1`.
- `-t linux|android` — select the target library set. The default is `linux`.
- `-a v68|v75` — select the DSP architecture-version directory. The default is
  `v68`.

The legacy runner expects its installed `android/`, `linux/`, `v68/`, and
`v75/` sample libraries to be present. Use `make install` after the normal
Autotools build when the packaged installation layout is required. Its plugin
and deployment behavior is separate from `base_test`; do not use the legacy
`-U`, `-t`, or `-a` options with `builddir/bin/fastrpc_test`.
