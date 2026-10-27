// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef TEST_DOMAIN_H
#define TEST_DOMAIN_H

#include <stddef.h>

#ifndef NUM_DOMAINS
#define NUM_DOMAINS 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns non-zero when domain_id is a supported FastRPC domain. */
int test_domain_is_supported(int domain_id);

/* Returns the number of supported domains used for explicit-domain capacity. */
int test_domain_count(void);

/* Discovers running domains with usable FastRPC device nodes. */
int test_domain_discover(int domains[NUM_DOMAINS]);

/* Domain URI and name helpers retained under their existing public names. */
int test_utils_domain_uri(char *buf, size_t buflen);
int test_utils_domain_uri_for(int domain_id, char *buf, size_t buflen);
const char *test_utils_domain_name(void);
const char *test_utils_domain_name_for(int domain_id);

#ifdef __cplusplus
}
#endif

#endif /* TEST_DOMAIN_H */
