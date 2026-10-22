// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "e2e_test_utils.h"
#include "hap_example.h"

#include <stdbool.h>

int run_test(int domain_id, bool is_unsignedpd_enabled)
{
    remote_handle64 handle = (remote_handle64)-1;
    int ret = e2e_open(hap_example_URI, domain_id, is_unsignedpd_enabled,
                       hap_example_open, &handle);
    if (ret != AEE_SUCCESS)
        return ret;

    ret = hap_example_farf_runtime(handle, 0);
    if (ret == AEE_SUCCESS)
        ret = hap_example_farf_runtime(handle, 1);

    {
        int close_ret = e2e_close(handle, hap_example_close);
        if (ret == AEE_SUCCESS)
            ret = close_ret;
    }
    return ret;
}
