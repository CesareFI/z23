/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 *
 * test_jsonq — regressions for the jsonq stdin JSON path-query CLI
 * (tools/jsonq.c, built as build/bin/jsonq by `make jsonq`).
 *
 * Python is banned in this repository; nested command envelopes are
 * extracted with jsonq. These cases pin the exact stdout and exit codes of
 * the operators an agent shell actually relies on, including the container
 * shapes: count/keys must measure only the direct members of the selected
 * object or array — a nested container is one member, never a source of
 * extra counted keys. */

#include "test/test_core.h"
#include "util/spawn.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "test/jsonq_fixture_adapter.h"

static int test_jsonq_count_and_keys(void)
{
    int failures = 0;
    char buf[512] = {0};
    TEST("jsonq: count/keys measure direct members only") {
        ASSERT(jsonq_fixture_run("{\"a\":1,\"b\":{\"x\":1,\"y\":2},\"c\":[1,2]}",
                         (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "3" JSONQ_FIXTURE_EOL "") == 0);

        ASSERT(jsonq_fixture_run("{\"a\":1,\"b\":{\"x\":1}}", (const char *const[]){JSONQ_FIXTURE_BIN, "keys", "", NULL},
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "a" JSONQ_FIXTURE_EOL "b" JSONQ_FIXTURE_EOL "") == 0);

        /* A nested container under the queried path is one member; its own
         * keys must not leak into the parent count or key list. */
        ASSERT(jsonq_fixture_run("{\"b\":{\"x\":{\"deep\":1}}}", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "b", NULL},
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "1" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run("{\"b\":{\"x\":{\"deep\":1}}}", (const char *const[]){JSONQ_FIXTURE_BIN, "keys", "b", NULL},
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "x" JSONQ_FIXTURE_EOL "") == 0);

        /* Arrays were already correct: nested containers count as one
         * element each. Pin it so both container kinds stay honest. */
        ASSERT(jsonq_fixture_run("[1,[2,3],{\"x\":1}]", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL},
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "3" JSONQ_FIXTURE_EOL "") == 0);

        /* An object member whose value is an array: still one member. */
        ASSERT(jsonq_fixture_run("{\"a\":[1,2,3],\"b\":{\"c\":{\"d\":1}}}",
                         (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "2" JSONQ_FIXTURE_EOL "") == 0);

        /* Empty and mixed nested values stay direct members. */
        ASSERT(jsonq_fixture_run("{}", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "0" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run("{}", (const char *const[]){JSONQ_FIXTURE_BIN, "keys", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "") == 0);
        ASSERT(jsonq_fixture_run("[]", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "0" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run("{\"a\":[{\"hidden\":1}],\"b\":null,\"c\":[]}",
                         (const char *const[]){JSONQ_FIXTURE_BIN, "keys", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "a" JSONQ_FIXTURE_EOL "b" JSONQ_FIXTURE_EOL "c" JSONQ_FIXTURE_EOL "") == 0);
        PASS();
    } _test_next:;
    return failures;
}

static int test_jsonq_scalar_paths(void)
{
    int failures = 0;
    char buf[512] = {0};
    TEST("jsonq: get/has/eq/type address scalars inside nested shapes") {
        const char *doc = "{\"result\":{\"items\":[{\"id\":\"a\"},"
                          "{\"id\":\"b\"}],\"ok\":true},\"n\":7}";
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "get", "result.items[1].id", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "b" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "has", "result.ok", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "type", "result.items", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "array" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "eq", "result.ok", "true", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "get", "n", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "7" JSONQ_FIXTURE_EOL "") == 0);
        /* Missing path exits 1; malformed document exits 2. */
        ASSERT(jsonq_fixture_run(doc, (const char *const[]){JSONQ_FIXTURE_BIN, "get", "result.absent", NULL}, buf, sizeof(buf)) == 1);
        ASSERT(jsonq_fixture_run("{not json}", (const char *const[]){JSONQ_FIXTURE_BIN, "get", "a", NULL}, buf, sizeof(buf)) == 2);
        /* CRT text stdin must not hide malformed suffix bytes on Windows. */
        ASSERT(jsonq_fixture_run("{}\x1a" "junk", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 2);
        ASSERT(strcmp(buf, "") == 0);
        ASSERT(jsonq_fixture_run("{\r\n\"a\":1,\r\n\"b\":[]}\r\n", (const char *const[]){JSONQ_FIXTURE_BIN, "count", "", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "2" JSONQ_FIXTURE_EOL) == 0);
        PASS();
    } _test_next:;
    return failures;
}

static int test_jsonq_unwrap(void)
{
    int failures = 0;
    char buf[512] = {0};
    TEST("jsonq: unwrap prints a result envelope and refuses an error one") {
        ASSERT(jsonq_fixture_run("{\"result\":{\"ok\":true},\"error\":null}",
                         (const char *const[]){JSONQ_FIXTURE_BIN, "unwrap", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "{\"ok\":true}" JSONQ_FIXTURE_EOL "") == 0);
        ASSERT(jsonq_fixture_run("{\"result\":1,\"error\":{\"code\":-1}}",
                         (const char *const[]){JSONQ_FIXTURE_BIN, "unwrap", NULL}, buf, sizeof(buf)) == 2);
        ASSERT(jsonq_fixture_run("{\"result\":[1,2]}", (const char *const[]){JSONQ_FIXTURE_BIN, "unwrap", NULL}, buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "[1,2]" JSONQ_FIXTURE_EOL "") == 0);
        PASS();
    } _test_next:;
    return failures;
}

#if !defined(_WIN32)
static int jsonq_run_at_length(size_t length, char *out, size_t out_size)
{
    static const char prefix[] = "{\"v\":1}";
    if (length < sizeof prefix - 1) return -1;
    char dir[4096], input[4608];
    if (!test_mkdtemp(dir, sizeof dir, "jsonq_limit")) return -1;
    int result = -1;
    int n = snprintf(input, sizeof input, "%s/input.json", dir);
    if (n <= 0 || (size_t)n >= sizeof input) goto cleanup;
    FILE *file = fopen(input, "wb");
    if (!file) goto cleanup;
    char spaces[4096];
    memset(spaces, ' ', sizeof spaces);
    bool written = fwrite(prefix, 1, sizeof prefix - 1, file) ==
                   sizeof prefix - 1;
    size_t remaining = length - (sizeof prefix - 1);
    while (written && remaining > 0) {
        size_t chunk = remaining < sizeof spaces ? remaining : sizeof spaces;
        written = fwrite(spaces, 1, chunk, file) == chunk;
        remaining -= chunk;
    }
    if (fclose(file) != 0) written = false;
    if (!written) goto cleanup;
    const char *const argv[] = {
        "sh", "-c", "input=$1; exec \"$2\" get v < \"$input\"",
        "jsonq-limit", input, JSONQ_FIXTURE_BIN, NULL};
    result = zcl_spawn_capture(argv, out, out_size, 20000);
cleanup:
    if (test_rm_rf_recursive(dir) != 0) result = -1;
    return result;
}

static int test_jsonq_exact_input_limit(void)
{
    enum { INPUT_LIMIT = 16 << 20 };
    int failures = 0;
    char out[64] = {0};
    TEST("jsonq: exact input limit accepts, one byte beyond refuses") {
        ASSERT(jsonq_run_at_length(INPUT_LIMIT - 1, out, sizeof out) == 0);
        ASSERT(strcmp(out, "1" JSONQ_FIXTURE_EOL) == 0);
        ASSERT(jsonq_run_at_length(INPUT_LIMIT, out, sizeof out) == 0);
        ASSERT(strcmp(out, "1" JSONQ_FIXTURE_EOL) == 0);
        ASSERT(jsonq_run_at_length(INPUT_LIMIT + 1, out, sizeof out) == 2);
        PASS();
    } _test_next:;
    return failures;
}
#endif

int test_jsonq(void)
{
    int failures = 0;
    struct stat st;
    bool available = stat(JSONQ_FIXTURE_BIN, &st) == 0;
#if defined(_WIN32)
    available = available && (st.st_mode & S_IFREG) != 0;
#else
    available = available && (st.st_mode & S_IXUSR) != 0;
#endif
    if (!available) {
        printf("jsonq: required build/bin/jsonq missing (run: make jsonq)" JSONQ_FIXTURE_EOL "");
        return 1;
    }
    failures += test_jsonq_count_and_keys();
    failures += test_jsonq_scalar_paths();
    failures += test_jsonq_unwrap();
#if !defined(_WIN32)
    failures += test_jsonq_exact_input_limit();
#endif
    return failures;
}
