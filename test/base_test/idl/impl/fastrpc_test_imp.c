// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * fastrpc_test_imp.c - DSP-side implementation of the fastrpc_test interface.
 *
 * The QAIC-generated skeleton dispatches handle lifecycle calls and the
 * calculator and HAP sample operations implemented below.
 */

#define FARF_ERROR 1
#define FARF_HIGH 1
#define FARF_MEDIUM 0
#define FARF_LOW 0

#include "AEEStdErr.h"
#include "HAP_farf.h"
#include "fastrpc_test.h"

#define MAX_FASTRPC_TEST_HANDLES 8
#define FARF_RUNTIME_LOGGING_MASK 0x1f

static unsigned char g_handle_in_use[MAX_FASTRPC_TEST_HANDLES];

static int handle_is_valid(remote_handle64 h)
{
    return h < MAX_FASTRPC_TEST_HANDLES && g_handle_in_use[h];
}

int fastrpc_test_open(const char *uri, remote_handle64 *h)
{
    (void)uri;

    if (h == NULL)
        return AEE_EBADPARM;

    for (int i = 0; i < MAX_FASTRPC_TEST_HANDLES; i++) {
        if (!g_handle_in_use[i]) {
            g_handle_in_use[i] = 1;
            *h = (remote_handle64)i;
            HAP_setFARFRuntimeLoggingParams(FARF_RUNTIME_LOGGING_MASK, NULL, 0);
            return AEE_SUCCESS;
        }
    }

    return AEE_ENOMEMORY;
}

int fastrpc_test_close(remote_handle64 h)
{
    if (!handle_is_valid(h))
        return AEE_EBADPARM;

    g_handle_in_use[h] = 0;
    return AEE_SUCCESS;
}

int fastrpc_test_calculator_sum(remote_handle64 h, const int *values, int values_len,
                                int64_t *result)
{
    int64_t sum = 0;

    if (!handle_is_valid(h) || values == NULL || result == NULL || values_len <= 0)
        return AEE_EBADPARM;

    for (int i = 0; i < values_len; i++)
        sum += values[i];
    *result = sum;
    return AEE_SUCCESS;
}

int fastrpc_test_calculator_max(remote_handle64 h, const int *values, int values_len,
                                int *result)
{
    if (!handle_is_valid(h) || values == NULL || result == NULL || values_len <= 0)
        return AEE_EBADPARM;

    *result = values[0];
    for (int i = 1; i < values_len; i++) {
        if (values[i] > *result)
            *result = values[i];
    }
    return AEE_SUCCESS;
}

int fastrpc_test_hap_example_farf_runtime(remote_handle64 h, int enable)
{
    if (!handle_is_valid(h))
        return AEE_EBADPARM;

    HAP_setFARFRuntimeLoggingParams(enable ? FARF_RUNTIME_LOGGING_MASK : 0, NULL, 0);
    FARF(LOW, "FARF low logging %s", enable ? "enabled" : "disabled");
    FARF(MEDIUM, "FARF medium logging %s", enable ? "enabled" : "disabled");
    FARF(HIGH, "FARF high logging %s", enable ? "enabled" : "disabled");
    FARF(ERROR, "FARF error logging %s", enable ? "enabled" : "disabled");
    return AEE_SUCCESS;
}
