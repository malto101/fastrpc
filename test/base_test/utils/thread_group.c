// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "thread_group.h"

#include <errno.h>
#include <stdlib.h>

typedef struct {
    test_thread_group_t *group;
    test_thread_group_fn fn;
    void *context;
} thread_context_t;

static void *thread_start(void *opaque)
{
    thread_context_t *context = opaque;
    void *result;

    if (test_thread_group_wait_for_gate(context->group) != 0) {
        free(context);
        return NULL;
    }
    result = context->fn(context->context);
    free(context);
    return result;
}

int test_thread_group_init(test_thread_group_t *group, size_t count)
{
    if (!group || count == 0)
        return EINVAL;
    *group = (test_thread_group_t){ 0 };
    group->threads = calloc(count, sizeof(*group->threads));
    group->joined = calloc(count, sizeof(*group->joined));
    if (!group->threads || !group->joined) {
        free(group->threads);
        free(group->joined);
        *group = (test_thread_group_t){ 0 };
        return ENOMEM;
    }
    int error = pthread_mutex_init(&group->gate_mutex, NULL);
    if (error != 0) {
        free(group->threads);
        free(group->joined);
        *group = (test_thread_group_t){ 0 };
        return error;
    }
    error = pthread_cond_init(&group->gate_condition, NULL);
    if (error != 0) {
        pthread_mutex_destroy(&group->gate_mutex);
        free(group->threads);
        free(group->joined);
        *group = (test_thread_group_t){ 0 };
        return error;
    }
    group->count = count;
    group->gate_initialized = 1;
    return 0;
}

int test_thread_group_start(test_thread_group_t *group, test_thread_group_fn fn,
                           void *const contexts[])
{
    int result = 0;
    int error;

    if (!group || !fn || !group->gate_initialized || group->created != 0)
        return EINVAL;

    /* A joined group may be reused, but every batch starts behind a closed
     * gate so callers can create all workers before releasing them. */
    error = pthread_mutex_lock(&group->gate_mutex);
    if (error != 0)
        return error;
    group->gate_open = 0;
    error = pthread_mutex_unlock(&group->gate_mutex);
    if (error != 0)
        return error;

    for (size_t i = 0; i < group->count; i++) {
        thread_context_t *context = malloc(sizeof(*context));

        if (!context) {
            result = ENOMEM;
            break;
        }
        context->group = group;
        context->fn = fn;
        context->context = contexts ? contexts[i] : NULL;
        group->joined[i] = 0;
        error = pthread_create(&group->threads[i], NULL, thread_start, context);
        if (error != 0) {
            free(context);
            result = error;
            break;
        }
        group->created++;
    }

    if (result != 0 && group->created != 0) {
        (void)test_thread_group_open_gate(group);
        (void)test_thread_group_join(group);
    }
    return result;
}

int test_thread_group_open_gate(test_thread_group_t *group)
{
    int error;

    if (!group || !group->gate_initialized)
        return EINVAL;
    error = pthread_mutex_lock(&group->gate_mutex);
    if (error != 0)
        return error;
    group->gate_open = 1;
    error = pthread_cond_broadcast(&group->gate_condition);
    if (error == 0)
        error = pthread_mutex_unlock(&group->gate_mutex);
    else
        (void)pthread_mutex_unlock(&group->gate_mutex);
    return error;
}

int test_thread_group_wait_for_gate(test_thread_group_t *group)
{
    int error;

    if (!group || !group->gate_initialized)
        return EINVAL;
    error = pthread_mutex_lock(&group->gate_mutex);
    if (error != 0)
        return error;
    while (!group->gate_open) {
        error = pthread_cond_wait(&group->gate_condition, &group->gate_mutex);
        if (error != 0) {
            (void)pthread_mutex_unlock(&group->gate_mutex);
            return error;
        }
    }
    return pthread_mutex_unlock(&group->gate_mutex);
}

int test_thread_group_join(test_thread_group_t *group)
{
    int result = 0;

    if (!group || !group->gate_initialized)
        return EINVAL;
    if (group->created != 0) {
        int error = test_thread_group_open_gate(group);

        if (error != 0)
            return error;
    }
    for (size_t i = 0; i < group->created; i++) {
        int error;

        if (group->joined[i])
            continue;
        error = pthread_join(group->threads[i], NULL);
        if (error != 0) {
            if (result == 0)
                result = error;
            continue;
        }
        group->joined[i] = 1;
    }
    if (result == 0)
        group->created = 0;
    return result;
}

void test_thread_group_destroy(test_thread_group_t *group)
{
    if (!group)
        return;
    if (group->created != 0)
        (void)test_thread_group_open_gate(group);
    (void)test_thread_group_join(group);
    if (group->gate_initialized) {
        pthread_cond_destroy(&group->gate_condition);
        pthread_mutex_destroy(&group->gate_mutex);
    }
    free(group->threads);
    free(group->joined);
    *group = (test_thread_group_t){ 0 };
}
