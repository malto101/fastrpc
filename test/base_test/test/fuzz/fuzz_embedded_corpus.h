// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file fuzz_embedded_corpus.h
 * @brief Shared layout for build-time-embedded fuzz seed corpora.
 *
 * test/fuzz/CMakeLists.txt generates one `fuzz_embedded_corpus_<entry_point>`
 * instance of these types per test/fuzz/<suite>/corpus/<entry_point>/ dir
 * (see cmake/embed_corpus.cmake) — a handful of tiny seed files baked
 * straight into the binary. root_all_tests.c uses them to self-seed a
 * suite's default corpus directory on disk the first time --fuzz runs and
 * finds it missing, so the binary doesn't depend on a separate `adb push`
 * of test/fuzz/<suite>/corpus/ just to get started.
 *
 * This is only ever meant for the small, checked-in seed set — not a
 * substitute for a real, growing corpus directory kept across fuzzing runs.
 */

#ifndef FUZZ_EMBEDDED_CORPUS_H
#define FUZZ_EMBEDDED_CORPUS_H

#include <stddef.h>

struct fuzz_embedded_seed {
    const char *name;             /* original seed file name, used verbatim when materialized */
    const unsigned char *data;
    size_t size;
};

struct fuzz_embedded_corpus {
    const char *entry_point;
    const struct fuzz_embedded_seed *seeds;
    size_t seed_count;
};

#endif /* FUZZ_EMBEDDED_CORPUS_H */
