// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "calculator.h"
#include "AEEStdErr.h"

#include <stdint.h>
#include <stdlib.h>

int calculator_open(const char *uri, remote_handle64 *handle)
{
    (void)uri;
    if (handle == NULL)
        return AEE_EBADPARM;
    *handle = (remote_handle64)malloc(1);
    return *handle != (remote_handle64)0 ? AEE_SUCCESS : AEE_ENOMEMORY;
}

int calculator_close(remote_handle64 handle)
{
    free((void *)handle);
    return AEE_SUCCESS;
}

int calculator_sum(remote_handle64 handle, const int *vec, int vec_len, int64_t *result)
{
    int64_t sum = 0;
    (void)handle;
    if (vec == NULL || result == NULL || vec_len < 0)
        return AEE_EBADPARM;
    for (int i = 0; i < vec_len; i++)
        sum += vec[i];
    *result = sum;
    return AEE_SUCCESS;
}

int calculator_max(remote_handle64 handle, const int *vec, int vec_len, int *result)
{
    int maximum;
    (void)handle;
    if (vec == NULL || result == NULL || vec_len <= 0)
        return AEE_EBADPARM;
    maximum = vec[0];
    for (int i = 1; i < vec_len; i++)
        if (vec[i] > maximum)
            maximum = vec[i];
    *result = maximum;
    return AEE_SUCCESS;
}
