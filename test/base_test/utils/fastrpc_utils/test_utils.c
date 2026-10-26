// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "test_utils.h"
#include "AEEStdErr.h"
#include "fastrpc_common.h"
#include "fastrpc_test.h"
#include "remote.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define REMOTEPROC_CLASS_PATH "/sys/class/remoteproc"

typedef struct {
    int domain_id;
    const char *domain_name;
    const char *uri_suffix;
    const char *remoteproc_names[2];
    const char *devnodes[2];
} domain_probe_t;

/* Keep all domain identity and discovery information in one table. */
static const domain_probe_t domain_probes[] = {
    { ADSP_DOMAIN_ID, ADSP_DOMAIN_NAME, ADSP_DOMAIN,
      { "adsp", NULL },
      { "/dev/fastrpc-adsp", "/dev/fastrpc-adsp-secure" } },
    { MDSP_DOMAIN_ID, MDSP_DOMAIN_NAME, MDSP_DOMAIN,
      { "mdsp", NULL },
      { "/dev/fastrpc-mdsp", "/dev/fastrpc-mdsp-secure" } },
    { SDSP_DOMAIN_ID, SDSP_DOMAIN_NAME, SDSP_DOMAIN,
      { "sdsp", "slpi" },
      { "/dev/fastrpc-sdsp", "/dev/fastrpc-sdsp-secure" } },
    { CDSP_DOMAIN_ID, CDSP_DOMAIN_NAME, CDSP_DOMAIN,
      { "cdsp", NULL },
      { "/dev/fastrpc-cdsp", "/dev/fastrpc-cdsp-secure" } },
    { CDSP1_DOMAIN_ID, CDSP1_DOMAIN_NAME, CDSP1_DOMAIN,
      { "cdsp1", NULL },
      { "/dev/fastrpc-cdsp1", "/dev/fastrpc-cdsp1-secure" } },
    { GDSP0_DOMAIN_ID, GDSP0_DOMAIN_NAME, GDSP0_DOMAIN,
      { "gpdsp0", "gdsp0" },
      { "/dev/fastrpc-gdsp0", "/dev/fastrpc-gdsp0-secure" } },
    { GDSP1_DOMAIN_ID, GDSP1_DOMAIN_NAME, GDSP1_DOMAIN,
      { "gpdsp1", "gdsp1" },
      { "/dev/fastrpc-gdsp1", "/dev/fastrpc-gdsp1-secure" } },
};

#define DOMAIN_PROBE_COUNT (sizeof(domain_probes) / sizeof(domain_probes[0]))
#define DOMAIN_ALIAS_COUNT \
    (sizeof(domain_probes[0].remoteproc_names) / sizeof(domain_probes[0].remoteproc_names[0]))
#define DOMAIN_DEVNODE_COUNT \
    (sizeof(domain_probes[0].devnodes) / sizeof(domain_probes[0].devnodes[0]))

static const domain_probe_t *find_domain_probe(int domain_id)
{
    for (size_t i = 0; i < DOMAIN_PROBE_COUNT; i++) {
        if (domain_probes[i].domain_id == domain_id)
            return &domain_probes[i];
    }

    return NULL;
}

static int domain_probe_index(int domain_id)
{
    for (size_t i = 0; i < DOMAIN_PROBE_COUNT; i++) {
        if (domain_probes[i].domain_id == domain_id)
            return (int)i;
    }

    return -1;
}

_Static_assert(DOMAIN_PROBE_COUNT <= NUM_DOMAINS,
               "domain_ids[] cannot hold every supported domain");

test_config_t g_test_config = {
    .domain_id = DEFAULT_DOMAIN_ID,
    .domain_ids = { 0 },
    .domain_count = 0,
    .unsigned_pd = 1,
    .silent_mode = 0,
    .list_mode = TEST_LIST_NONE,
    .any_tags = { NULL }, /* populated by --any-tags */
    .any_tag_count = 0,
    .all_tags = { NULL }, /* populated by --all-tags          */
    .all_tag_count = 0,
};

static int read_text_file(const char *path, char *buf, size_t buflen)
{
    FILE *file;
    size_t length;

    file = fopen(path, "r");
    if (!file)
        return -1;

    length = fread(buf, 1, buflen - 1, file);
    if (ferror(file)) {
        fclose(file);
        return -1;
    }

    fclose(file);
    buf[length] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static int remoteproc_name_matches(const char *name, const char *key)
{
    return name != NULL && key != NULL && strcmp(name, key) == 0;
}

static int domain_has_devnode(const domain_probe_t *probe)
{
    struct stat st;

    for (size_t i = 0; i < DOMAIN_DEVNODE_COUNT; i++) {
        if (probe->devnodes[i] != NULL && stat(probe->devnodes[i], &st) == 0 && S_ISCHR(st.st_mode))
            return 1;
    }

    return 0;
}

static int discover_domains(int domains[NUM_DOMAINS])
{
    int running[DOMAIN_PROBE_COUNT] = { 0 };
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(REMOTEPROC_CLASS_PATH);
    if (!dir) {
        fprintf(stderr, "[test_config] unable to scan %s: %s\n",
                REMOTEPROC_CLASS_PATH, strerror(errno));
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        char name[128];
        char state[64];

        if (strncmp(entry->d_name, "remoteproc", strlen("remoteproc")) != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s/name", REMOTEPROC_CLASS_PATH, entry->d_name);
        if (read_text_file(path, name, sizeof(name)) != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s/state", REMOTEPROC_CLASS_PATH, entry->d_name);
        if (read_text_file(path, state, sizeof(state)) != 0)
            continue;

        for (char *p = name; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        for (char *p = state; *p; p++)
            *p = (char)tolower((unsigned char)*p);

        if (strstr(state, "running") == NULL)
            continue;

        for (size_t i = 0; i < DOMAIN_PROBE_COUNT; i++) {
            for (size_t j = 0; j < DOMAIN_ALIAS_COUNT &&
                            domain_probes[i].remoteproc_names[j]; j++) {
                if (remoteproc_name_matches(name, domain_probes[i].remoteproc_names[j])) {
                    running[i] = 1;
                    break;
                }
            }
        }
    }

    closedir(dir);

    for (size_t i = 0; i < DOMAIN_PROBE_COUNT; i++) {
        if (running[i] && domain_has_devnode(&domain_probes[i]))
            domains[count++] = domain_probes[i].domain_id;
    }

    return count;
}

static int add_explicit_domain(int domain_id)
{
    for (int i = 0; i < g_test_config.domain_count; i++) {
        if (g_test_config.domain_ids[i] == domain_id)
            return 0;
    }

    if (g_test_config.domain_count >= (int)DOMAIN_PROBE_COUNT)
        return -1;

    g_test_config.domain_ids[g_test_config.domain_count++] = domain_id;
    return 0;
}

static int parse_domain_id(const char *value, int *domain_id)
{
    char *end;
    long parsed;

    if (!value || !domain_id || value[0] == '\0')
        return -1;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || !end || *end != '\0')
        return -1;

    if (parsed < INT_MIN || parsed > INT_MAX)
        return -1;

    if (domain_probe_index((int)parsed) >= 0) {
        *domain_id = (int)parsed;
        return 0;
    }

    return -1;
}

static int parse_boolean(const char *value, int *parsed_value)
{
    char *end;
    long parsed;

    if (!value || !parsed_value || value[0] == '\0')
        return -1;

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || !end || *end != '\0' || (parsed != 0 && parsed != 1))
        return -1;

    *parsed_value = (int)parsed;
    return 0;
}

static int set_list_mode(test_list_mode_t mode)
{
    if (g_test_config.list_mode != TEST_LIST_NONE && g_test_config.list_mode != mode) {
        fprintf(stderr, "[test_config] only one test listing mode may be selected\n");
        return -1;
    }

    g_test_config.list_mode = mode;
    return 0;
}

static int parse_list_mode(const char *value)
{
    if (strcmp(value, "tests") == 0)
        return set_list_mode(TEST_LIST_TESTS);
    if (strcmp(value, "groups") == 0)
        return set_list_mode(TEST_LIST_GROUPS);
    if (strcmp(value, "tags") == 0)
        return set_list_mode(TEST_LIST_TAGS);

    fprintf(stderr, "[test_config] invalid -l value '%s'; expected tests, groups, or tags\n",
            value);
    return -1;
}

static int arguments_request_help(int argc, const char **argv)
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return 1;
    }
    return 0;
}

static int validate_unity_arguments(int argc, const char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *argument = argv[i];

        if (strcmp(argument, "-v") == 0 || strcmp(argument, "-s") == 0 ||
            strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0)
            continue;

        if (strcmp(argument, "-g") == 0 || strcmp(argument, "-G") == 0 ||
            strcmp(argument, "-n") == 0 || strcmp(argument, "-N") == 0) {
            if (++i >= argc || argv[i][0] == '\0' || argv[i][0] == '-')
                return -1;
            continue;
        }

        if (strcmp(argument, "-r") == 0) {
            char *end;
            unsigned long repeat;

            if (++i >= argc || argv[i][0] == '\0' || argv[i][0] == '-')
                return -1;
            errno = 0;
            repeat = strtoul(argv[i], &end, 10);
            if (errno != 0 || !end || *end != '\0' || repeat == 0 || repeat > UINT_MAX)
                return -1;
            continue;
        }

        return -1;
    }
    return 0;
}

int test_config_init(int argc, const char **argv, int *out_argc, const char ***out_argv)
{
    const char **filtered = malloc((size_t)argc * sizeof(const char *));
    int domains_explicit = 0;
    int unsigned_pd_explicit = 0;
    int silent_explicit = 0;

    if (!filtered) {
        fprintf(stderr, "[test_config] malloc failed\n");
        return -1;
    }

    int fi = 0;

    g_test_config.domain_count = 0;
    g_test_config.domain_id = DEFAULT_DOMAIN_ID;
    g_test_config.unsigned_pd = 1;
    g_test_config.silent_mode = 0;
    g_test_config.list_mode = TEST_LIST_NONE;
    g_test_config.any_tag_count = 0;
    g_test_config.all_tag_count = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[test_config] -l requires tests, groups, or tags\n");
                free(filtered);
                return -1;
            }
            if (parse_list_mode(argv[++i]) != 0) {
                free(filtered);
                return -1;
            }
        } else if (strcmp(argv[i], "--list-tests") == 0) {
            if (set_list_mode(TEST_LIST_TESTS) != 0) {
                free(filtered);
                return -1;
            }
        } else if (strcmp(argv[i], "--list-groups") == 0) {
            if (set_list_mode(TEST_LIST_GROUPS) != 0) {
                free(filtered);
                return -1;
            }
        } else if (strcmp(argv[i], "--list-tags") == 0) {
            if (set_list_mode(TEST_LIST_TAGS) != 0) {
                free(filtered);
                return -1;
            }
        } else if (strcmp(argv[i], "-d") == 0) {
            int domain_id;

            if (i + 1 >= argc) {
                fprintf(stderr, "[test_config] -d requires a domain ID\n");
                free(filtered);
                return -1;
            }

            if (parse_domain_id(argv[++i], &domain_id) != 0 ||
                add_explicit_domain(domain_id) != 0) {
                fprintf(stderr, "[test_config] invalid domain ID: %s\n", argv[i]);
                free(filtered);
                return -1;
            }
            domains_explicit = 1;
        } else if (strcmp(argv[i], "-u") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "[test_config] -u requires 0 or 1\n");
                free(filtered);
                return -1;
            }
            if (parse_boolean(argv[++i], &g_test_config.unsigned_pd) != 0) {
                fprintf(stderr, "[test_config] invalid unsigned-PD value: %s\n", argv[i]);
                free(filtered);
                return -1;
            }
            unsigned_pd_explicit = 1;
        } else if (strcmp(argv[i], "--silent") == 0) {
            /*
             * --silent  — suppress the per-test header/dot/footer block for
             * tests that pass; failed and skipped tests still print their
             * full block. Implemented by buffering each test's stdout output
             * and discarding it on a pass (see UnityTestRunnerWithFileOutput
             * in unity_fixture_file_output.c). Consumed here; never
             * forwarded to UnityMain().
             */
            g_test_config.silent_mode = 1;
            silent_explicit = 1;
        } else if (strcmp(argv[i], "--any-tags") == 0) {
            /*
             * --any-tags <tag>  (repeatable, up to TEST_CONFIG_MAX_TAGS)
             *
             * OR-mode filter: a test runs if its metadata annotation
             * contains AT LEAST ONE of the --any-tags values.
             *
             * Consumed here; never forwarded to UnityMain().
             */
            if (i + 1 >= argc) {
                fprintf(stderr, "[test_config] --any-tags requires a tag\n");
                free(filtered);
                return -1;
            }
            if (g_test_config.any_tag_count < TEST_CONFIG_MAX_TAGS) {
                g_test_config.any_tags[g_test_config.any_tag_count++] = argv[++i];
            } else {
                fprintf(stderr,
                        "[test_config] Warning: --any-tags limit (%d) reached, "
                        "ignoring '%s'\n",
                        TEST_CONFIG_MAX_TAGS, argv[++i]);
            }
        } else if (strcmp(argv[i], "--all-tags") == 0) {
            /*
             * --all-tags <tag>  (repeatable, up to TEST_CONFIG_MAX_TAGS)
             *
             * AND-mode filter: a test runs only if its metadata annotation
             * contains ALL of the --all-tags values.
             *
             * Consumed here; never forwarded to UnityMain().
             */
            if (i + 1 >= argc) {
                fprintf(stderr, "[test_config] --all-tags requires a tag\n");
                free(filtered);
                return -1;
            }
            if (g_test_config.all_tag_count < TEST_CONFIG_MAX_TAGS) {
                g_test_config.all_tags[g_test_config.all_tag_count++] = argv[++i];
            } else {
                fprintf(stderr,
                        "[test_config] Warning: --all-tags limit (%d) reached, "
                        "ignoring '%s'\n",
                        TEST_CONFIG_MAX_TAGS, argv[++i]);
            }
        } else {
            filtered[fi++] = argv[i];
        }
    }

    if (validate_unity_arguments(fi, filtered) != 0) {
        fprintf(stderr, "[test_config] invalid Unity option or missing option value\n");
        free(filtered);
        return -1;
    }

    if (g_test_config.list_mode != TEST_LIST_NONE || arguments_request_help(fi, filtered)) {
        *out_argc = fi;
        *out_argv = filtered;
        return 0;
    }

    if (unsigned_pd_explicit)
        printf("[test_config] unsigned_pd = %d\n", g_test_config.unsigned_pd);
    if (silent_explicit)
        printf("[test_config] silent_mode = %d\n", g_test_config.silent_mode);
    for (int i = 0; i < g_test_config.any_tag_count; i++)
        printf("[test_config] any-tag filter[%d] = %s\n", i, g_test_config.any_tags[i]);
    for (int i = 0; i < g_test_config.all_tag_count; i++)
        printf("[test_config] all-tag filter[%d] = %s\n", i, g_test_config.all_tags[i]);

    if (g_test_config.domain_count == 0) {
        g_test_config.domain_count = discover_domains(g_test_config.domain_ids);
        if (g_test_config.domain_count <= 0) {
            fprintf(stderr,
                    "[test_config] no running DSP domains with FastRPC device nodes found\n");
            free(filtered);
            return -1;
        }
    }

    printf("[test_config] %s domains:",
           domains_explicit ? "selected" : "discovered");
    for (int i = 0; i < g_test_config.domain_count; i++) {
        const char *domain_name = test_utils_domain_name_for(g_test_config.domain_ids[i]);

        printf(" %s(%d)", domain_name ? domain_name : "<invalid>",
               g_test_config.domain_ids[i]);
    }
    putchar('\n');

    *out_argc = fi;
    *out_argv = filtered;
    return 0;
}

int test_utils_domain_uri_for(int domain_id, char *buf, size_t buflen)
{
    const domain_probe_t *probe = find_domain_probe(domain_id);
    int written;

    if (buf == NULL || buflen == 0)
        return -1;

    buf[0] = '\0';
    if (probe == NULL)
        return -1;

    written = snprintf(buf, buflen, "%s%s", fastrpc_test_URI, probe->uri_suffix);
    if (written < 0 || (size_t)written >= buflen) {
        buf[0] = '\0';
        return -1;
    }

    return 0;
}

int test_utils_domain_uri(char *buf, size_t buflen)
{
    return test_utils_domain_uri_for(g_test_config.domain_id, buf, buflen);
}

const char *test_utils_domain_name_for(int domain_id)
{
    const domain_probe_t *probe = find_domain_probe(domain_id);

    return probe != NULL ? probe->domain_name : NULL;
}

const char *test_utils_domain_name(void)
{
    return test_utils_domain_name_for(g_test_config.domain_id);
}

int test_utils_setup_dsp_lib_path(void)
{
    static int done = 0;
    const char *existing;
    size_t existing_len;
    size_t search_len;
    size_t separator_len;
    size_t total_len;
    char *path;

    if (done)
        return 0;

    existing = getenv("DSP_LIBRARY_PATH");
    if (existing == NULL)
        existing = "";

    existing_len = strlen(existing);
    search_len = strlen(SKEL_SEARCH_PATH);
    separator_len = existing_len != 0 ? 1 : 0;

    if (existing_len > SIZE_MAX - separator_len ||
        existing_len + separator_len > SIZE_MAX - search_len ||
        existing_len + separator_len + search_len > SIZE_MAX - 1) {
        fprintf(stderr, "[test_utils] DSP_LIBRARY_PATH is too long\n");
        return -1;
    }

    total_len = existing_len + separator_len + search_len + 1;
    path = malloc(total_len);
    if (path == NULL) {
        fprintf(stderr, "[test_utils] unable to allocate DSP_LIBRARY_PATH\n");
        return -1;
    }

    memcpy(path, existing, existing_len);
    if (separator_len != 0)
        path[existing_len] = ';';
    memcpy(path + existing_len + separator_len, SKEL_SEARCH_PATH, search_len + 1);

    if (setenv("DSP_LIBRARY_PATH", path, 1) != 0) {
        fprintf(stderr, "[test_utils] setenv DSP_LIBRARY_PATH failed: %s\n", strerror(errno));
        free(path);
        return -1;
    }

    printf("[test_utils] DSP_LIBRARY_PATH=%s\n", path);
    free(path);
    done = 1;
    return 0;
}

void test_config_free_argv(const char **argv)
{
    free((void *)argv);
}

int test_utils_enable_unsigned_pd(int domain)
{
    struct remote_rpc_control_unsigned_module attr;
    memset(&attr, 0, sizeof(attr));
    attr.domain = domain;
    attr.enable = 1;

    return remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, &attr, sizeof(attr));
}

const char *test_utils_err_str(int err)
{
    switch (err) {
    case AEE_SUCCESS:
        return "AEE_SUCCESS";
    case AEE_EUNKNOWN:
        return "AEE_EUNKNOWN";
    case AEE_EFAILED:
        return "AEE_EFAILED";
    case AEE_ENOMEMORY:
        return "AEE_ENOMEMORY";
    case AEE_EBADPARM:
        return "AEE_EBADPARM";
    case AEE_EINVALIDFORMAT:
        return "AEE_EINVALIDFORMAT";
    case AEE_EUNSUPPORTED:
        return "AEE_EUNSUPPORTED";
    case AEE_ENOSUCH:
        return "AEE_ENOSUCH";
    case AEE_ECONNREFUSED:
        return "AEE_ECONNREFUSED";
    case AEE_EINVHANDLE:
        return "AEE_EINVHANDLE";
    case AEE_EBUSY:
        return "AEE_EBUSY";
    case AEE_EINVALIDDOMAIN:
        return "AEE_EINVALIDDOMAIN";
    case AEE_EINVALIDDEVICE:
        return "AEE_EINVALIDDEVICE";
    case AEE_ENOTINITIALIZED:
        return "AEE_ENOTINITIALIZED";
    case AEE_EUNSIGNEDMOD:
        return "AEE_EUNSIGNEDMOD";
    case AEE_ENOSUCHFILE:
        return "AEE_ENOSUCHFILE";
    case AEE_ENOSUCHMOD:
        return "AEE_ENOSUCHMOD";
    case AEE_EBADDOMAIN:
        return "AEE_EBADDOMAIN";
    case AEE_ERPC:
        return "AEE_ERPC";
    case AEE_ENORPCMEMORY:
        return "AEE_ENORPCMEMORY";
    default:
        return "(unknown error)";
    }
}

/* Stores the most recent error code reported via REPORT_ERROR_CODE().
 * INT_MIN means "no code set yet for this test". */
static volatile int s_last_error_code = INT_MIN;

void test_utils_set_last_error_code(int err) { s_last_error_code = err; }

int test_utils_get_last_error_code(void) { return s_last_error_code; }

void test_utils_reset_last_error_code(void) { s_last_error_code = INT_MIN; }

/* Accumulation buffer; excess beyond TEST_UTILS_MAX_ERROR_CODES is silently dropped. */
static int s_accumulated_codes[TEST_UTILS_MAX_ERROR_CODES];
static int s_accumulated_count = 0;

void test_utils_accumulate_error_code(int err)
{
    if (s_accumulated_count < TEST_UTILS_MAX_ERROR_CODES) {
        s_accumulated_codes[s_accumulated_count++] = err;
    }
}

const int *test_utils_get_accumulated_error_codes(int *count)
{
    if (count) {
        *count = s_accumulated_count;
    }
    return s_accumulated_codes;
}

void test_utils_reset_accumulated_error_codes(void) { s_accumulated_count = 0; }

int test_utils_setup_test(const char *tag)
{
    int ret = test_utils_setup_dsp_lib_path();

    if (ret != 0) {
        printf("[%s] FATAL: test_utils_setup_dsp_lib_path() failed\n", tag);
        return -1;
    }

    if (g_test_config.unsigned_pd) {
        ret = test_utils_enable_unsigned_pd(g_test_config.domain_id);
        if (ret != 0) {
            printf("[%s] enable_unsigned_pd returned 0x%x (%s) - continuing\n", tag, ret,
                   test_utils_err_str(ret));
        }
    } else {
        printf("[%s] unsigned PD disabled via -u 0, skipping\n", tag);
    }

    return 0;
}
