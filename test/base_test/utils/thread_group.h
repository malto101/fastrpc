// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BASE_TEST_THREAD_GROUP_H
#define BASE_TEST_THREAD_GROUP_H

#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*test_thread_group_fn)(void *context);

typedef struct {
    pthread_t *threads;
    unsigned char *joined;
    size_t count;
    size_t created;
    pthread_mutex_t gate_mutex;
    pthread_cond_t gate_condition;
    int gate_open;
    int gate_initialized;
} test_thread_group_t;

/* The group must be zero-initialized and not previously initialized. */
int test_thread_group_init(test_thread_group_t *group, size_t count);
int test_thread_group_start(test_thread_group_t *group, test_thread_group_fn fn,
                           void *const contexts[]);
int test_thread_group_open_gate(test_thread_group_t *group);
/* Opens the gate before joining, so start() followed by join() is safe. */
int test_thread_group_join(test_thread_group_t *group);
void test_thread_group_destroy(test_thread_group_t *group);

/* A worker may call this before doing work to synchronize start. */
int test_thread_group_wait_for_gate(test_thread_group_t *group);

#ifdef __cplusplus
}
#endif

#endif /* BASE_TEST_THREAD_GROUP_H */
