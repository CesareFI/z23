/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The queued CHECK dimension derives its closure and asks admission
 *          before any test child. Launch counts come from the forked step,
 *          not from a flag the decider writes for itself. */

#include "test/test_core.h"

#include "test/proof_ticket_fixture.h"

#include "dev_proof.h"
#include "platform/time_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

enum { SEAM_RAW = 9, SEAM_CLASS = 8, SEAM_PATH = 512 };

struct seam_meter {
    struct zcl_dev_proof_check_result out;
    int64_t wall_us;
    int64_t cpu_us;
};

static int64_t seam_cpu_us(const struct rusage *before, const struct rusage *after)
{
    int64_t user = (int64_t)(after->ru_utime.tv_sec - before->ru_utime.tv_sec) *
                       1000000 +
                   ((int64_t)after->ru_utime.tv_usec - before->ru_utime.tv_usec);
    int64_t sys = (int64_t)(after->ru_stime.tv_sec - before->ru_stime.tv_sec) *
                      1000000 +
                  ((int64_t)after->ru_stime.tv_usec - before->ru_stime.tv_usec);
    return user + sys;
}

static void seam_fill(uint8_t raw[SEAM_RAW][32],
                      struct zcl_dev_proof_check_inputs *in)
{
    memset(in, 0, sizeof(*in));
    for (int i = 0; i < SEAM_RAW; i++) {
        memset(raw[i], (unsigned char)(0x31 + i), 32);
        raw[i][0] = (unsigned char)(0xA0 + i);
    }
    in->unit = "proof-check-group";
    in->source_cas = raw[0];
    in->dependency = raw[1];
    in->harness = raw[2];
    in->flags = raw[3];
    in->environment = raw[4];
    in->build_graph = raw[5];
    in->toolchain = raw[6];
    in->policy = raw[7];
    in->changed = raw[8];
}

static bool seam_script(const char *path, unsigned ran)
{
    FILE *f = fopen(path, "w");
    int n;
    if (!f) return false;
    n = fprintf(f,
                "#!/bin/sh\n"
                "dd if=/dev/zero bs=1048576 count=64 2>/dev/null | "
                "sha256sum >/dev/null || exit 1\n"
                "printf '%%s\\n' 'SUITE VERDICT mode=cold groups_total=%u "
                "groups_ran=%u groups_cached=0 groups_gated=0 "
                "groups_failed=0 self_skips=0 env_unobserved=0'\n",
                ran, ran);
    if (n < 0 || fclose(f) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool seam_dir(char *dir, size_t cap, char *binary, size_t binary_cap,
                     unsigned ran)
{
    char *made = test_mkdtemp(dir, cap, "z23-check-seam");
    if (!made) return false;
    if (snprintf(binary, binary_cap, "%s/child.sh", dir) >= (int)binary_cap)
        return false;
    return seam_script(binary, ran);
}

static bool seam_align(struct ptf *f, const struct zcl_dev_proof_check_inputs *in,
                       struct vcs_component_proof_key_v1 *key,
                       struct vcs_proof_reuse_policy *policy)
{
    if (!zcl_dev_proof_check_closure_derive(in, key)) return false;
    *policy = f->policy;
    memcpy(policy->policy_root, key->roots[VCS_CPK_POLICY], 32);
    return true;
}

static bool seam_pass_sync(struct ptf *f, int signer,
                           const struct vcs_component_proof_key_v1 *key,
                           struct ptf_spec spec)
{
    struct vcs_proof_sync_report rep;
    if (!ptf_emit(f, signer, key, spec, NULL, NULL)) return false;
    if (!ptf_sync(f, signer, 0, &rep)) return false;
    return rep.outcome == VCS_PROOF_SYNC_ADVANCED;
}

static bool seam_quorum(struct ptf *f, const struct vcs_component_proof_key_v1 *key)
{
    return seam_pass_sync(f, PTF_A, key, ptf_pass()) &&
           seam_pass_sync(f, PTF_B, key, ptf_pass());
}

/* used[0] is nonzero. Local so the assertion text stays specific. */
static bool proof_root_used(const struct vcs_proof_reuse_decision *d)
{
    uint8_t any = 0;
    for (size_t i = 0; i < 32; i++) any |= d->used[0][i];
    return any != 0;
}

static bool seam_run(const struct zcl_dev_proof_check_inputs *in,
                     const char *dir, const char *binary, uint32_t selected,
                     const struct vcs_proof_receiver *rx,
                     const struct vcs_proof_candidate_domain *domain,
                     const struct vcs_proof_reuse_policy *policy,
                     struct seam_meter *meter)
{
    struct rusage before, after;
    int64_t began;
    if (getrusage(RUSAGE_CHILDREN, &before) != 0) return false;
    began = platform_time_monotonic_us();
    if (!zcl_dev_proof_check_dimensions(in, dir, dir, binary, selected, rx,
                                        domain, policy, &meter->out))
        return false;
    meter->wall_us = platform_time_monotonic_us() - began;
    if (getrusage(RUSAGE_CHILDREN, &after) != 0) return false;
    meter->cpu_us = seam_cpu_us(&before, &after);
    return true;
}

static int seam_case_hit_cold(void)
{
    int failures = 0;
    TEST_CASE("check seam: qualified hit launches no child; cold executes") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct vcs_proof_ticket_class cls[SEAM_CLASS];
        struct vcs_proof_reuse_decision decision;
        struct seam_meter hit, cold;
        char hit_dir[SEAM_PATH], cold_dir[SEAM_PATH], hit_bin[SEAM_PATH],
            cold_bin[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        ASSERT(seam_dir(hit_dir, sizeof(hit_dir), hit_bin, sizeof(hit_bin), 1));
        ASSERT(seam_dir(cold_dir, sizeof(cold_dir), cold_bin, sizeof(cold_bin), 1));
        ASSERT(seam_run(&in, hit_dir, hit_bin, 1, f.rx, &f.domain, &policy, &hit));
        ASSERT(seam_run(&in, cold_dir, cold_bin, 1, NULL, NULL, NULL, &cold));
        ASSERT(ptf_decide(&f, &key, VCS_PROOF_ACTION_CHECK, &policy, cls,
                          SEAM_CLASS, &decision));
        ASSERT_EQ(decision.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT(decision.used_count >= 1u);
        ASSERT(proof_root_used(&decision));
        ASSERT(memcmp(hit.out.receipt_root, decision.used[0], 32) == 0);
        ASSERT(hit.out.ok);
        ASSERT_EQ(hit.out.test_children, 0u);
        ASSERT_EQ(hit.out.reused, 1u);
        ASSERT_EQ(hit.out.ran, 0u);
        ASSERT(!hit.out.log_present);
        ASSERT(cold.out.ok);
        ASSERT_EQ(cold.out.test_children, 1u);
        ASSERT_EQ(cold.out.reused, 0u);
        ASSERT_EQ(cold.out.ran, 1u);
        ASSERT(cold.out.log_present);
        ASSERT(cold.out.test_children > hit.out.test_children);
        ASSERT(cold.wall_us > hit.wall_us);
        ASSERT(cold.cpu_us > hit.cpu_us);
        printf("check_seam hit_children=%u hit_reused=%u hit_wall_us=%lld "
               "hit_child_cpu_us=%lld cold_children=%u cold_reused=%u "
               "cold_wall_us=%lld cold_child_cpu_us=%lld observation_bound=1\n",
               hit.out.test_children, hit.out.reused, (long long)hit.wall_us,
               (long long)hit.cpu_us, cold.out.test_children, cold.out.reused,
               (long long)cold.wall_us, (long long)cold.cpu_us);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_wide(void)
{
    int failures = 0;
    TEST_CASE("check seam: wider selection still executes") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter wide;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 2));
        ASSERT(seam_run(&in, dir, binary, 2, f.rx, &f.domain, &policy, &wide));
        ASSERT(wide.out.ok);
        ASSERT_EQ(wide.out.test_children, 1u);
        ASSERT_EQ(wide.out.reused, 0u);
        ASSERT_EQ(wide.out.ran, 2u);
        printf("check_seam case=wide children=%u reused=%u ok=%d\n",
               wide.out.test_children, wide.out.reused, wide.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_domain(void)
{
    int failures = 0;
    TEST_CASE("check seam: same-domain signer does not skip") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        uint8_t verifiers[1][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        memcpy(verifiers[0], f.pub[PTF_AUTHOR], 32);
        policy.verifiers = (const uint8_t (*)[32])verifiers;
        policy.verifier_count = 1;
        policy.quorum = 1;
        ASSERT(seam_pass_sync(&f, PTF_AUTHOR, &key, ptf_pass()));
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=domain children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_conflict(void)
{
    int failures = 0;
    TEST_CASE("check seam: observation conflict blocks") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_pass_sync(&f, PTF_A, &key, ptf_pass()));
        ASSERT(seam_pass_sync(&f, PTF_B, &key, ptf_pass()));
        ASSERT(seam_pass_sync(&f, PTF_C, &key, ptf_fail()));
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(!run.out.ok);
        ASSERT_EQ(run.out.test_children, 0u);
        ASSERT_EQ(run.out.reused, 0u);
        ASSERT(!run.out.log_present);
        ASSERT(strstr(run.out.why, VCS_PROOF_OBSERVATION_CONFLICT) != NULL);
        printf("check_seam case=conflict children=%u reused=%u ok=%d why=%s\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0,
               run.out.why);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_changed(void)
{
    int failures = 0;
    TEST_CASE("check seam: changed closure executes") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        raw[0][31] ^= 0x01;
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=changed children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_incomplete(void)
{
    int failures = 0;
    TEST_CASE("check seam: incomplete closure executes") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        in.source_cas = NULL;
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=incomplete children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_reused_basis(void)
{
    int failures = 0;
    TEST_CASE("check seam: REUSED-basis tickets do not skip") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        uint8_t root[32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct ptf_spec reused;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(ptf_emit(&f, PTF_A, &key, ptf_pass(), NULL, root));
        reused = ptf_pass();
        reused.basis = VCS_PROOF_BASIS_REUSED;
        reused.basis_ref = root;
        ASSERT(seam_pass_sync(&f, PTF_A, &key, reused));
        ASSERT(seam_pass_sync(&f, PTF_B, &key, reused));
        ASSERT(seam_pass_sync(&f, PTF_C, &key, reused));
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=reused_basis children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_stale(void)
{
    int failures = 0;
    TEST_CASE("check seam: stale observations do not skip") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        policy.now_unix = 1790000000ull + 100000ull;
        policy.max_age_seconds = 10;
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=stale children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_revoked(void)
{
    int failures = 0;
    TEST_CASE("check seam: revoked verifiers do not skip") {
        struct ptf f;
        uint8_t raw[SEAM_RAW][32];
        uint8_t revoked[2][32];
        struct zcl_dev_proof_check_inputs in;
        struct vcs_component_proof_key_v1 key;
        struct vcs_proof_reuse_policy policy;
        struct seam_meter run;
        char dir[SEAM_PATH], binary[SEAM_PATH];
        ASSERT(ptf_init(&f));
        seam_fill(raw, &in);
        ASSERT(seam_align(&f, &in, &key, &policy));
        ASSERT(seam_quorum(&f, &key));
        memcpy(revoked[0], f.pub[PTF_A], 32);
        memcpy(revoked[1], f.pub[PTF_B], 32);
        policy.revoked = (const uint8_t (*)[32])revoked;
        policy.revoked_count = 2;
        ASSERT(seam_dir(dir, sizeof(dir), binary, sizeof(binary), 1));
        ASSERT(seam_run(&in, dir, binary, 1, f.rx, &f.domain, &policy, &run));
        ASSERT(run.out.ok);
        ASSERT_EQ(run.out.test_children, 1u);
        ASSERT_EQ(run.out.reused, 0u);
        printf("check_seam case=revoked children=%u reused=%u ok=%d\n",
               run.out.test_children, run.out.reused, run.out.ok ? 1 : 0);
        ptf_free(&f);
    } TEST_END
    return failures;
}

static int seam_case_cached(void)
{
    int failures = 0;
    TEST_CASE("check seam: groups_cached without a vouch is not reuse") {
        char dir[SEAM_PATH];
        char *made = test_mkdtemp(dir, sizeof(dir), "z23-check-seam-log");
        char path[SEAM_PATH];
        struct zcl_dev_proof_dimension dim = {.selected = 1};
        FILE *f;
        ASSERT(made != NULL);
        ASSERT(snprintf(path, sizeof(path), "%s/verdict.log", dir) <
               (int)sizeof(path));
        f = fopen(path, "w");
        ASSERT(f != NULL);
        ASSERT(fprintf(f,
                       "SUITE VERDICT mode=cold groups_total=1 groups_ran=0 "
                       "groups_cached=1 groups_gated=0 groups_failed=0 "
                       "self_skips=0 env_unobserved=0\n") > 0);
        ASSERT(fclose(f) == 0);
        ASSERT(!zcl_dev_proof_test_log_account(path, &dim));
        printf("check_seam case=groups_cached accounted=0\n");
    } TEST_END
    return failures;
}

int test_proof_ticket_check_seam(void)
{
    int failures = 0;
    failures += seam_case_hit_cold();
    failures += seam_case_wide();
    failures += seam_case_domain();
    failures += seam_case_conflict();
    failures += seam_case_changed();
    failures += seam_case_incomplete();
    failures += seam_case_reused_basis();
    failures += seam_case_stale();
    failures += seam_case_revoked();
    failures += seam_case_cached();
    return failures;
}
