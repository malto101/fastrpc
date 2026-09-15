// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * libFuzzer harness for dspqueue_create()'s parameter validation.
 *
 * dspqueue_create() validates its queue out-pointer, domain, flags, and
 * queue-size arguments entirely in CPU code before touching rpcmem/the DSP
 * (see src/dspqueue/dspqueue_cpu.c), so this is reachable and meaningful to
 * fuzz without a live DSP device.
 */

#include <stdint.h>
#include <string.h>

#include "dspqueue.h"

struct fuzz_input {
  int32_t domain;
  uint32_t flags;
  uint32_t req_queue_size;
  uint32_t resp_queue_size;
  uint8_t null_queue_ptr;
};

int fuzz_dspqueue_create_run(const uint8_t *data, size_t size) {
  struct fuzz_input in;

  if (size < sizeof(in)) {
    return 0;
  }
  memcpy(&in, data, sizeof(in));

  dspqueue_t queue = NULL;
  dspqueue_t *queue_out = (in.null_queue_ptr & 1) ? NULL : &queue;

  int nErr = dspqueue_create(in.domain, in.flags, in.req_queue_size,
                              in.resp_queue_size, NULL, NULL, NULL, queue_out);

  if (nErr == 0 && queue_out != NULL && queue != NULL) {
    dspqueue_close(queue);
  }

  return 0;
}
