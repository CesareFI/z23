/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0
 *
 * test_dev_proof_observation_env — regressions for the proof worker's
 * per-proof observation/testcache store environment.
 *
 * The push proof's test dimension now runs the runner with
 * --cold-audit --emit-observations (see dp_test_dimension_argv in
 * tools/dev/dev_proof.c). Cold audit opens a verdict store, and observation
 * emission writes signed leaves into a content-addressed CAS. Both must be
 * private to the proof:
 *
 *   - A shared or candidate-writable verdict store is exactly what
 *     proof_prepare_environment() scrubs ZCL_TESTCACHE_STORE_ROOT for.
 *   - A signed observation leaf is durable evidence; it must never share a
 *     tree with the unsigned, candidate-writable PASS records the runner
 *     also writes into its verdict store.
 *
 * proof_observation_env_open() therefore points ZCL_TESTCACHE_STORE_ROOT and
 * ZCL_DEV_OBSERVATION_STORE at per-pair directories under the proof state
 * (<state>/testcache.<key> scratch, removed with the proof;
 * <state>/observations.<key> durable), and proof_observation_env_close()
 * unsets both on every worker exit so a resident daemon's environment never
 * carries one proof's stores into the next — the same failure mode the
 * stress-env group (test_dev_proof_stress_env.c) pins for
 * ZCL_STRESS_TESTS. Driving a full proof cycle here would rebuild a
 * generation and run dimensions just to watch environment variables; this
 * proves the narrower, load-bearing facts directly through the ZCL_TESTING
 * seam onto the exact helpers proof_worker() calls. */

#include "test/test_core.h"

#include "dev_proof.h"
#include "dev_proof_observation.h"

#include <stdlib.h>
#include <string.h>

#define DPOE_KEY "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef01-cafe01"

static void dpoe_clear_env(void)
{
    (void)unsetenv("ZCL_TESTCACHE_STORE_ROOT");
    (void)unsetenv(ZCL_DEV_OBSERVATION_STORE_ENV);
}

static int test_dpoe_absent_becomes_private_pair(void)
{
    int failures = 0;
    char root[4096];
    test_make_tmpdir(root, sizeof(root), "proof_obs_env", "absent");
    TEST_CASE("dev_proof_obs_env: absent before, per-pair private stores after") {
        dpoe_clear_env();
        ASSERT(getenv("ZCL_TESTCACHE_STORE_ROOT") == NULL);
        ASSERT(getenv(ZCL_DEV_OBSERVATION_STORE_ENV) == NULL);

        char why[160] = {0};
        ASSERT(zcl_dev_proof_test_observation_env_prepare(
                   root, DPOE_KEY, why, sizeof(why)));
        ASSERT(why[0] == '\0');

        char expect[4096];
        ASSERT(snprintf(expect, sizeof(expect), "%s/testcache.%s",
                        root, DPOE_KEY) > 0);
        const char *verdict_store = getenv("ZCL_TESTCACHE_STORE_ROOT");
        ASSERT(verdict_store != NULL);
        ASSERT_STR_EQ(verdict_store, expect);
        ASSERT(snprintf(expect, sizeof(expect), "%s/observations.%s",
                        root, DPOE_KEY) > 0);
        const char *observation_store =
            getenv(ZCL_DEV_OBSERVATION_STORE_ENV);
        ASSERT(observation_store != NULL);
        ASSERT_STR_EQ(observation_store, expect);

        /* The two stores are distinct trees: durable signed observations
         * never share content-addressed space with unsigned verdict
         * records. */
        ASSERT(strcmp(verdict_store, observation_store) != 0);
    }
    TEST_END
    dpoe_clear_env();
    (void)test_rm_rf_recursive(root);
    return failures;
}

static int test_dpoe_restore_removes_both(void)
{
    int failures = 0;
    char root[4096];
    test_make_tmpdir(root, sizeof(root), "proof_obs_env", "restore");
    TEST_CASE("dev_proof_obs_env: restore removes both knobs again") {
        dpoe_clear_env();
        ASSERT(zcl_dev_proof_test_observation_env_prepare(
                   root, DPOE_KEY, NULL, 0));
        ASSERT(getenv("ZCL_TESTCACHE_STORE_ROOT") != NULL);
        ASSERT(getenv(ZCL_DEV_OBSERVATION_STORE_ENV) != NULL);

        ASSERT(zcl_dev_proof_test_observation_env_restore(root, DPOE_KEY));
        ASSERT(getenv("ZCL_TESTCACHE_STORE_ROOT") == NULL);
        ASSERT(getenv(ZCL_DEV_OBSERVATION_STORE_ENV) == NULL);
    }
    TEST_END
    dpoe_clear_env();
    (void)test_rm_rf_recursive(root);
    return failures;
}

static int test_dpoe_unconditional_overwrite(void)
{
    int failures = 0;
    char root[4096];
    test_make_tmpdir(root, sizeof(root), "proof_obs_env", "overwrite");
    TEST_CASE("dev_proof_obs_env: stale inherited values are overwritten") {
        /* A prior tool, or a resident daemon's very first environment, could
         * have exported a shared store here. The proof must not defer to it:
         * the whole point is that the test dimension no longer depends on
         * what invoked this binary. */
        ASSERT(setenv("ZCL_TESTCACHE_STORE_ROOT", "/shared/verdict-store",
                      1) == 0);
        ASSERT(setenv(ZCL_DEV_OBSERVATION_STORE_ENV, "/shared/observations",
                      1) == 0);

        char why[160] = {0};
        ASSERT(zcl_dev_proof_test_observation_env_prepare(
                   root, DPOE_KEY, why, sizeof(why)));
        char expect[4096];
        ASSERT(snprintf(expect, sizeof(expect), "%s/testcache.%s",
                        root, DPOE_KEY) > 0);
        ASSERT_STR_EQ(getenv("ZCL_TESTCACHE_STORE_ROOT"), expect);
        ASSERT(snprintf(expect, sizeof(expect), "%s/observations.%s",
                        root, DPOE_KEY) > 0);
        ASSERT_STR_EQ(getenv(ZCL_DEV_OBSERVATION_STORE_ENV), expect);
    }
    TEST_END
    dpoe_clear_env();
    (void)test_rm_rf_recursive(root);
    return failures;
}

static int test_dpoe_refusal_leaves_env_untouched(void)
{
    int failures = 0;
    char root[4096];
    test_make_tmpdir(root, sizeof(root), "proof_obs_env", "refuse");
    TEST_CASE("dev_proof_obs_env: invalid pair refuses by name, env untouched") {
        ASSERT(setenv("ZCL_TESTCACHE_STORE_ROOT", "/sentinel/verdict",
                      1) == 0);
        ASSERT(setenv(ZCL_DEV_OBSERVATION_STORE_ENV, "/sentinel/observations",
                      1) == 0);

        char why[160] = {0};
        /* A key containing '/' would escape the per-pair directory. */
        ASSERT(!zcl_dev_proof_test_observation_env_prepare(
                   root, "bad/key", why, sizeof(why)));
        ASSERT(why[0] != '\0');
        ASSERT(!zcl_dev_proof_test_observation_env_prepare(
                   "", DPOE_KEY, why, sizeof(why)));
        ASSERT(!zcl_dev_proof_test_observation_env_prepare(
                   root, "", why, sizeof(why)));
        ASSERT(!zcl_dev_proof_test_observation_env_prepare(
                   NULL, DPOE_KEY, why, sizeof(why)));

        /* A refused prepare must not have partial-set the environment. */
        ASSERT_STR_EQ(getenv("ZCL_TESTCACHE_STORE_ROOT"), "/sentinel/verdict");
        ASSERT_STR_EQ(getenv(ZCL_DEV_OBSERVATION_STORE_ENV),
                      "/sentinel/observations");
    }
    TEST_END
    dpoe_clear_env();
    (void)test_rm_rf_recursive(root);
    return failures;
}

int test_dev_proof_observation_env(void)
{
    int failures = 0;
    failures += test_dpoe_absent_becomes_private_pair();
    failures += test_dpoe_restore_removes_both();
    failures += test_dpoe_unconditional_overwrite();
    failures += test_dpoe_refusal_leaves_env_untouched();
    return failures;
}
