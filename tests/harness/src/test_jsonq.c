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

#define JSONQ_BIN "build/bin/jsonq"

/* Write the document under a private tmpdir, run
 * `sh -c "jsonq <args> < file"`, capture stdout. */
static int jsonq_run(const char *doc, const char *args, char *buf, size_t cap)
{
    char dir[4096], path[4608], cmd[5120];
    test_make_tmpdir(dir, sizeof(dir), "jsonq", "run");
    int n = snprintf(path, sizeof(path), "%s/in.json", dir);
    if (n <= 0 || (size_t)n >= sizeof(path)) return -1;
    FILE *f = fopen(path, "w");
    if (!f || fputs(doc, f) < 0 || fclose(f) != 0) return -1;
    n = snprintf(cmd, sizeof(cmd), "%s %s < %s", JSONQ_BIN, args, path);
    if (n <= 0 || (size_t)n >= sizeof(cmd)) return -1;
    const char *argv[] = {"sh", "-c", cmd, NULL};
    int rc = zcl_spawn_capture(argv, buf, cap, 5000);
    (void)test_rm_rf_recursive(dir);
    return rc;
}

static int test_jsonq_count_and_keys(void)
{
    int failures = 0;
    char buf[512] = {0};
    TEST("jsonq: count/keys measure direct members only") {
        ASSERT(jsonq_run("{\"a\":1,\"b\":{\"x\":1,\"y\":2},\"c\":[1,2]}",
                         "count ''", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "3\n") == 0);

        ASSERT(jsonq_run("{\"a\":1,\"b\":{\"x\":1}}", "keys ''",
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "a\nb\n") == 0);

        /* A nested container under the queried path is one member; its own
         * keys must not leak into the parent count or key list. */
        ASSERT(jsonq_run("{\"b\":{\"x\":{\"deep\":1}}}", "count b",
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "1\n") == 0);
        ASSERT(jsonq_run("{\"b\":{\"x\":{\"deep\":1}}}", "keys b",
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "x\n") == 0);

        /* Arrays were already correct: nested containers count as one
         * element each. Pin it so both container kinds stay honest. */
        ASSERT(jsonq_run("[1,[2,3],{\"x\":1}]", "count ''",
                         buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "3\n") == 0);

        /* An object member whose value is an array: still one member. */
        ASSERT(jsonq_run("{\"a\":[1,2,3],\"b\":{\"c\":{\"d\":1}}}",
                         "count ''", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "2\n") == 0);
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
        ASSERT(jsonq_run(doc, "get result.items[1].id", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "b\n") == 0);
        ASSERT(jsonq_run(doc, "has result.ok", buf, sizeof(buf)) == 0);
        ASSERT(jsonq_run(doc, "type result.items", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "array\n") == 0);
        ASSERT(jsonq_run(doc, "eq result.ok true", buf, sizeof(buf)) == 0);
        ASSERT(jsonq_run(doc, "get n", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "7\n") == 0);
        /* Missing path exits 1; malformed document exits 2. */
        ASSERT(jsonq_run(doc, "get result.absent", buf, sizeof(buf)) == 1);
        ASSERT(jsonq_run("{not json}", "get a", buf, sizeof(buf)) == 2);
        PASS();
    } _test_next:;
    return failures;
}

static int test_jsonq_unwrap(void)
{
    int failures = 0;
    char buf[512] = {0};
    TEST("jsonq: unwrap prints a result envelope and refuses an error one") {
        ASSERT(jsonq_run("{\"result\":{\"ok\":true},\"error\":null}",
                         "unwrap", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "{\"ok\":true}\n") == 0);
        ASSERT(jsonq_run("{\"result\":1,\"error\":{\"code\":-1}}",
                         "unwrap", buf, sizeof(buf)) == 2);
        ASSERT(jsonq_run("{\"result\":[1,2]}", "unwrap", buf, sizeof(buf)) == 0);
        ASSERT(strcmp(buf, "[1,2]\n") == 0);
        PASS();
    } _test_next:;
    return failures;
}

int test_jsonq(void)
{
    int failures = 0;
    struct stat st;
    if (stat(JSONQ_BIN, &st) != 0 || (st.st_mode & S_IXUSR) == 0) {
        printf("jsonq: build/bin/jsonq missing (run: make jsonq) — skipped\n");
        return 0;
    }
    failures += test_jsonq_count_and_keys();
    failures += test_jsonq_scalar_paths();
    failures += test_jsonq_unwrap();
    return failures;
}
