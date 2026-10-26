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
    group->contexts = calloc(count, sizeof(*group->contexts));
    if (!group->threads || !group->contexts) {
        free(group->threads);
        free(group->contexts);
        *group = (test_thread_group_t){ 0 };
        return ENOMEM;
    }
    if (pthread_mutex_init(&group->gate_mutex, NULL) != 0) {
        free(group->threads);
        free(group->contexts);
        *group = (test_thread_group_t){ 0 };
        return EINVAL;
    }
    if (pthread_cond_init(&group->gate_condition, NULL) != 0) {
        pthread_mutex_destroy(&group->gate_mutex);
        free(group->threads);
        free(group->contexts);
        *group = (test_thread_group_t){ 0 };
        return EINVAL;
    }
    group->count = count;
    group->gate_initialized = 1;
    return 0;
}

int test_thread_group_start(test_thread_group_t *group, test_thread_group_fn fn,
                           void *const contexts[])
{
    int result = 0;

    if (!group || !fn || !group->gate_initialized || group->created != 0)
        return EINVAL;

    /* A joined group may be reused, but every batch starts behind a closed
     * gate so callers can create all workers before releasing them. */
    if (pthread_mutex_lock(&group->gate_mutex) != 0)
        return EINVAL;
    group->gate_open = 0;
    pthread_mutex_unlock(&group->gate_mutex);

    for (size_t i = 0; i < group->count; i++) {
        thread_context_t *context = malloc(sizeof(*context));
        int error;

        if (!context) {
            result = ENOMEM;
            break;
        }
        context->group = group;
        context->fn = fn;
        context->context = contexts ? contexts[i] : NULL;
        group->contexts[i] = context->context;
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
    if (!group || !group->gate_initialized)
        return EINVAL;
    if (pthread_mutex_lock(&group->gate_mutex) != 0)
        return EINVAL;
    group->gate_open = 1;
    pthread_cond_broadcast(&group->gate_condition);
    pthread_mutex_unlock(&group->gate_mutex);
    return 0;
}

int test_thread_group_wait_for_gate(test_thread_group_t *group)
{
    if (!group || !group->gate_initialized)
        return EINVAL;
    if (pthread_mutex_lock(&group->gate_mutex) != 0)
        return EINVAL;
    while (!group->gate_open) {
        int error = pthread_cond_wait(&group->gate_condition, &group->gate_mutex);
        if (error != 0) {
            pthread_mutex_unlock(&group->gate_mutex);
            return error;
        }
    }
    pthread_mutex_unlock(&group->gate_mutex);
    return 0;
}

int test_thread_group_join(test_thread_group_t *group)
{
    int result = 0;

    if (!group || !group->gate_initialized)
        return EINVAL;
    for (size_t i = 0; i < group->created; i++) {
        int error = pthread_join(group->threads[i], NULL);
        if (error != 0 && result == 0)
            result = error;
    }
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
    free(group->contexts);
    *group = (test_thread_group_t){ 0 };
}
