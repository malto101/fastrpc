// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef FASTRPC_E2E_TEST_UTILS_H
#define FASTRPC_E2E_TEST_UTILS_H

#include "remote.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Build a generated interface URI with the selected DSP domain suffix. */
int e2e_build_uri(const char *base_uri, int domain_id, char *buf, size_t buflen);

/* Enable unsigned-PD loading when requested by the common test runner. */
int e2e_enable_unsigned_pd(int domain_id, bool enabled);

/* Open an interface and return its remote handle. */
int e2e_open(const char *base_uri, int domain_id, bool unsigned_pd,
             int (*open_fn)(const char *, remote_handle64 *), remote_handle64 *handle);

/* Close a handle, accepting the sentinel used by generated open wrappers. */
int e2e_close(remote_handle64 handle, int (*close_fn)(remote_handle64));

#ifdef __cplusplus
}
#endif

#endif
