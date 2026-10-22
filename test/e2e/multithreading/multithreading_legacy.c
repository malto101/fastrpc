// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "e2e_test_utils.h"
#include "multithreading.h"

#include <stdbool.h>

int run_test(int domain_id, bool is_unsignedpd_enabled)
{
    remote_handle64 handle = (remote_handle64)-1;
    int ret = e2e_open(multithreading_URI, domain_id, is_unsignedpd_enabled,
                       multithreading_open, &handle);
    if (ret != AEE_SUCCESS)
        return ret;

    ret = multithreading_parallel_sum(handle);
    if (ret == AEE_SUCCESS)
        ret = multithreading_barriers(handle);
    if (ret == AEE_SUCCESS)
        ret = multithreading_mutexes(handle);

    {
        int close_ret = e2e_close(handle, multithreading_close);
        if (ret == AEE_SUCCESS)
            ret = close_ret;
    }
    return ret;
}
