// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "AEEStdDef.h"
#include "AEEStdErr.h"
#include "multithreading.h"
#include "remote.h"

#include <stdlib.h>

#ifdef __hexagon__
#include "qurt.h"

#define E2E_THREAD_COUNT 4
#define E2E_STACK_SIZE 4096

struct sum_args {
    int value;
    int result;
};

static void sum_worker(void *arg)
{
    struct sum_args *args = (struct sum_args *)arg;
    args->result = args->value * args->value;
    qurt_thread_exit(QURT_EOK);
}

static void barrier_worker(void *arg)
{
    qurt_barrier_t *barrier = (qurt_barrier_t *)arg;
    qurt_barrier_wait(barrier);
    qurt_thread_exit(QURT_EOK);
}

static void mutex_worker(void *arg)
{
    qurt_mutex_t *mutex = (qurt_mutex_t *)arg;
    qurt_mutex_lock(mutex);
    qurt_mutex_unlock(mutex);
    qurt_thread_exit(QURT_EOK);
}
#endif

AEEResult multithreading_open(const char *uri, remote_handle64 *handle)
{
    (void)uri;
    if (handle == NULL)
        return AEE_EBADPARM;
    *handle = (remote_handle64)0xdead0002;
    return AEE_SUCCESS;
}

AEEResult multithreading_close(remote_handle64 handle)
{
    (void)handle;
    return AEE_SUCCESS;
}

AEEResult multithreading_parallel_sum(remote_handle64 handle)
{
    (void)handle;
#ifdef __hexagon__
    qurt_thread_t ids[E2E_THREAD_COUNT];
    qurt_thread_attr_t attrs[E2E_THREAD_COUNT];
    struct sum_args args[E2E_THREAD_COUNT];
    void *stacks[E2E_THREAD_COUNT] = {0};
    int status;
    int total = 0;

    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        stacks[i] = malloc(E2E_STACK_SIZE);
        if (stacks[i] == NULL)
            return AEE_ENOMEMORY;
        args[i].value = i + 1;
        args[i].result = 0;
        qurt_thread_attr_init(&attrs[i]);
        qurt_thread_attr_set_stack_addr(&attrs[i], stacks[i]);
        qurt_thread_attr_set_stack_size(&attrs[i], E2E_STACK_SIZE);
        status = qurt_thread_create(&ids[i], &attrs[i], sum_worker, &args[i]);
        if (status != QURT_EOK)
            return AEE_EFAILED;
    }
    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        status = qurt_thread_join(ids[i], NULL);
        if (status != QURT_EOK && status != QURT_ENOTHREAD)
            return AEE_EFAILED;
        total += args[i].result;
        free(stacks[i]);
    }
    return total == 30 ? AEE_SUCCESS : AEE_EFAILED;
#else
    return AEE_SUCCESS;
#endif
}

AEEResult multithreading_barriers(remote_handle64 handle)
{
    (void)handle;
#ifdef __hexagon__
    qurt_barrier_t barrier;
    qurt_thread_t ids[E2E_THREAD_COUNT];
    qurt_thread_attr_t attrs[E2E_THREAD_COUNT];
    void *stacks[E2E_THREAD_COUNT] = {0};
    int status;

    qurt_barrier_init(&barrier, E2E_THREAD_COUNT + 1);
    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        stacks[i] = malloc(E2E_STACK_SIZE);
        if (stacks[i] == NULL)
            return AEE_ENOMEMORY;
        qurt_thread_attr_init(&attrs[i]);
        qurt_thread_attr_set_stack_addr(&attrs[i], stacks[i]);
        qurt_thread_attr_set_stack_size(&attrs[i], E2E_STACK_SIZE);
        status = qurt_thread_create(&ids[i], &attrs[i], barrier_worker, &barrier);
        if (status != QURT_EOK)
            return AEE_EFAILED;
    }
    qurt_barrier_wait(&barrier);
    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        status = qurt_thread_join(ids[i], NULL);
        if (status != QURT_EOK && status != QURT_ENOTHREAD)
            return AEE_EFAILED;
        free(stacks[i]);
    }
    qurt_barrier_destroy(&barrier);
#endif
    return AEE_SUCCESS;
}

AEEResult multithreading_mutexes(remote_handle64 handle)
{
    (void)handle;
#ifdef __hexagon__
    qurt_mutex_t mutex;
    qurt_thread_t ids[E2E_THREAD_COUNT];
    qurt_thread_attr_t attrs[E2E_THREAD_COUNT];
    void *stacks[E2E_THREAD_COUNT] = {0};
    int status;

    qurt_mutex_init(&mutex);
    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        stacks[i] = malloc(E2E_STACK_SIZE);
        if (stacks[i] == NULL)
            return AEE_ENOMEMORY;
        qurt_thread_attr_init(&attrs[i]);
        qurt_thread_attr_set_stack_addr(&attrs[i], stacks[i]);
        qurt_thread_attr_set_stack_size(&attrs[i], E2E_STACK_SIZE);
        status = qurt_thread_create(&ids[i], &attrs[i], mutex_worker, &mutex);
        if (status != QURT_EOK)
            return AEE_EFAILED;
    }
    for (int i = 0; i < E2E_THREAD_COUNT; i++) {
        status = qurt_thread_join(ids[i], NULL);
        if (status != QURT_EOK && status != QURT_ENOTHREAD)
            return AEE_EFAILED;
        free(stacks[i]);
    }
    qurt_mutex_destroy(&mutex);
#endif
    return AEE_SUCCESS;
}
