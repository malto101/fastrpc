// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "e2e_test_utils.h"
#include "AEEStdErr.h"
#include "remote.h"
#include "test_utils.h"

#include <stdio.h>
#include <string.h>

int e2e_build_uri(const char *base_uri, int domain_id, char *buf, size_t buflen)
{
    const char *suffix;

    if (base_uri == NULL || buf == NULL || buflen == 0)
        return AEE_EBADPARM;

    switch (domain_id) {
    case ADSP_DOMAIN_ID:
        suffix = ADSP_DOMAIN;
        break;
    case MDSP_DOMAIN_ID:
        suffix = MDSP_DOMAIN;
        break;
    case SDSP_DOMAIN_ID:
        suffix = SDSP_DOMAIN;
        break;
    case CDSP_DOMAIN_ID:
        suffix = CDSP_DOMAIN;
        break;
    case CDSP1_DOMAIN_ID:
        suffix = CDSP1_DOMAIN;
        break;
    case GDSP0_DOMAIN_ID:
        suffix = GDSP0_DOMAIN;
        break;
    case GDSP1_DOMAIN_ID:
        suffix = GDSP1_DOMAIN;
        break;
    default:
        return AEE_EBADPARM;
    }

    if (snprintf(buf, buflen, "%s%s", base_uri, suffix) < 0 ||
        strlen(base_uri) + strlen(suffix) >= buflen)
        return AEE_EBADSIZE;

    return AEE_SUCCESS;
}

int e2e_enable_unsigned_pd(int domain_id, bool enabled)
{
    if (!enabled)
        return AEE_SUCCESS;
    return test_utils_enable_unsigned_pd(domain_id);
}

int e2e_open(const char *base_uri, int domain_id, bool unsigned_pd,
             int (*open_fn)(const char *, remote_handle64 *), remote_handle64 *handle)
{
    char uri[512];
    int ret;

    if (open_fn == NULL || handle == NULL)
        return AEE_EBADPARM;

    ret = e2e_enable_unsigned_pd(domain_id, unsigned_pd);
    if (ret != AEE_SUCCESS)
        return ret;

    ret = e2e_build_uri(base_uri, domain_id, uri, sizeof(uri));
    if (ret != AEE_SUCCESS)
        return ret;

    *handle = (remote_handle64)-1;
    return open_fn(uri, handle);
}

int e2e_close(remote_handle64 handle, int (*close_fn)(remote_handle64))
{
    if (close_fn == NULL || handle == (remote_handle64)-1)
        return AEE_SUCCESS;
    return close_fn(handle);
}
