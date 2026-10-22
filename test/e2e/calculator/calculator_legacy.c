// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "calculator.h"
#include "e2e_test_utils.h"

#include <stdbool.h>
#include <stdint.h>

int run_test(int domain_id, bool is_unsignedpd_enabled)
{
    remote_handle64 handle = (remote_handle64)-1;
    const int values[] = {0, 1, 2, 3, 4};
    int64_t sum = 0;
    int maximum = 0;
    int ret;

    ret = e2e_open(calculator_URI, domain_id, is_unsignedpd_enabled,
                   calculator_open, &handle);
    if (ret != AEE_SUCCESS)
        return ret;

    ret = calculator_sum(handle, values, 5, &sum);
    if (ret == AEE_SUCCESS && sum != 10)
        ret = AEE_EFAILED;
    if (ret == AEE_SUCCESS)
        ret = calculator_max(handle, values, 5, &maximum);
    if (ret == AEE_SUCCESS && maximum != 4)
        ret = AEE_EFAILED;

    {
        int close_ret = e2e_close(handle, calculator_close);
        if (ret == AEE_SUCCESS)
            ret = close_ret;
    }
    return ret;
}
