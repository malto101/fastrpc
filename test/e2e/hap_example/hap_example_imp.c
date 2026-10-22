// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "hap_example.h"
#include "AEEStdErr.h"

#define FARF_LOW 1
#define FARF_MEDIUM 1
#define FARF_HIGH 1
#define FARF_ERROR 1
#define FARF_FATAL 1
#include "HAP_farf.h"

AEEResult hap_example_open(const char *uri, remote_handle64 *handle)
{
    (void)uri;
    if (handle == NULL)
        return AEE_EBADPARM;
    *handle = (remote_handle64)0xdead0001;
    return AEE_SUCCESS;
}

AEEResult hap_example_close(remote_handle64 handle)
{
    (void)handle;
    return AEE_SUCCESS;
}

AEEResult hap_example_farf_runtime(remote_handle64 handle, int enable)
{
    (void)handle;
#if !defined(__hexagon__)
    {
        const char *file_name = "hap_example_imp.c";
        HAP_setFARFRuntimeLoggingParams(enable ? 0x1f : 0, &file_name, 1);
    }
#endif
    FARF(LOW, "FARF low logging %s", enable ? "enabled" : "disabled");
    FARF(MEDIUM, "FARF medium logging %s", enable ? "enabled" : "disabled");
    FARF(HIGH, "FARF high logging %s", enable ? "enabled" : "disabled");
    FARF(ERROR, "FARF error logging %s", enable ? "enabled" : "disabled");
    FARF(FATAL, "FARF fatal logging %s", enable ? "enabled" : "disabled");
    FARF(RUNTIME_LOW, "FARF runtime low logging");
    FARF(RUNTIME_MEDIUM, "FARF runtime medium logging");
    FARF(RUNTIME_HIGH, "FARF runtime high logging");
    FARF(RUNTIME_ERROR, "FARF runtime error logging");
    FARF(RUNTIME_FATAL, "FARF runtime fatal logging");
    return AEE_SUCCESS;
}
