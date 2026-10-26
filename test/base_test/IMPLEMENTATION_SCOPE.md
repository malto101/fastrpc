# base_test documentation scope

This document records the approved documentation scope for the reviewed
`b163af5` base-test snapshot. It is a scope record, not a test implementation
plan.

## Included in this change

- Make the integrated quickstart explicitly AArch64 (cross-compiled
  `aarch64-linux-gnu`, with native AArch64 called out separately).
- Make standalone commands begin with `cd test/base_test` and use paths
  relative to that directory.
- Document complete device-build prerequisites, including explicit
  `HEXAGON_CLANG` and `HEXAGON_LD` inputs.
- Add a concise feature matrix for the suites and cases present in this
  snapshot.
- Document the supported CLI options, domain IDs, expected statuses, and
  common build/deployment/runtime troubleshooting.
- Document how to add a case, a unit/feature suite, and an E2E suite using the
  current CMake and Unity registration points.
- Keep process-isolation and worker implementation details out of the main
  test README; the README describes user-visible behavior instead.
- Clarify the difference between the legacy `test/fastrpc_test` runner and
  the CMake `test/base_test` runner.

## Deferred items

The following are intentionally outside this documentation change:

- Expanding test coverage or adding new test cases, suites, IDL methods, or
  deployment artifacts.
- Changing the set of supported DSP domains, domain limits, discovery rules,
  domain metadata, or unsigned/signed-PD behavior.
- Runtime optimization, process-isolation redesign, worker protocol changes,
  output/reporting changes, or test-runner implementation changes.
- Adding or expanding deployment support for other operating systems,
  architectures, toolchains, or device transport mechanisms beyond the
  existing Linux/Android AArch64 paths.
- Changes to CMake, Autotools, QAIC generation, source code, generated IDL,
  CI, daemon behavior, or device configuration.
- Coverage collection, performance benchmarking, automated device discovery,
  or emulator/host-side execution support.
- Replacing or restructuring contribution policy, coding standards, or the
  repository's broader build documentation.
