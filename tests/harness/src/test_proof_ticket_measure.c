/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Deterministic reuse measurement over issuer checkpoints, and the
 *          per-change admission report over interface contracts.
 *
 * Measurement: 6 candidates (one cold baseline, then candidates changing
 * 1-10% of 200 obligations plus one shared header edit) against three
 * independent issuers with their own signed logs. The candidate author and
 * the same-uid local signer "prove" everything they touch, one issuer
 * signs REUSED provenance for hits, one artifact is tampered in the CAS and
 * one issuer contradicts an honest PASS. Prints one parseable line and
 * requires zero false hits and >95% reuse of unchanged obligations.
 *
 * Admission: a callee component with callers and an unrelated component,
 * under a private implementation edit, a header/ABI edit, an unknown scope
 * and a conflict; each prints the owner's per-change report line. */

#include "test/test_core.h"

#include "test/proof_ticket_fixture.h"

#include "platform/time_compat.h"
#include "vcs/package_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#define PTM_UNITS 200u
#define PTM_CANDIDATES 6u
#define PTM_HEADER_UNITS 60u
#define PTM_FLAKY_UNIT 17u
#define PTM_BROKEN_UNIT 42u
#define PTM_TAMPER_UNIT 101u
#define PTM_CAP 64u

enum { PTM_WHY_DOMAIN = 0, PTM_WHY_REUSED, PTM_WHY_UNCOVERED, PTM_WHY_OTHER,
       PTM_WHY_ARTIFACT, PTM_WHY_COUNT };

static const char *const ptm_why_names[PTM_WHY_COUNT] = {
    "signer_in_candidate_domain", "reused_not_independent",
    "not_checkpointed", "other_ineligible", "artifact_bytes_mismatch",
};

struct ptm {
    struct ptf f;
    uint32_t version[PTM_UNITS];
    uint32_t header;
    uint64_t rng;
    uint32_t candidate;
    uint32_t turn;
    bool changed[PTM_UNITS];
    uint64_t obligations, eligible_proofs, reused, fresh, refused;
    uint64_t unchanged, unchanged_reused;
    uint64_t false_hits, why[PTM_WHY_COUNT];
    uint64_t tickets_classified, decide_us, full_log_bytes;
    uint64_t fixture_wall_us, fixture_process_cpu_us;
    uint64_t process_peak_rss_kib, io_read_ops, io_write_ops;
    bool io_observed;
};

static uint32_t ptm_rand(struct ptm *s)
{
    s->rng = s->rng * 6364136223846793005ull + 1442695040888963407ull;
    return (uint32_t)(s->rng >> 33);
}

static enum vcs_proof_action_class ptm_class(uint32_t u)
{
    return u % 4u == 0 ? VCS_PROOF_ACTION_CHECK : VCS_PROOF_ACTION_BUILD;
}

static bool ptm_truly_passes(const struct ptm *s, uint32_t u)
{
    return !(u == PTM_BROKEN_UNIT && (s->version[u] & 1u));
}

static void ptm_key(const struct ptm *s, uint32_t u,
                    struct vcs_component_proof_key_v1 *k)
{
    char text[96];
    *k = s->f.base;
    snprintf(text, sizeof(text), "unit/%u", u);
    ptf_root(VCS_CPK_UNIT_ID, text, k->roots[VCS_CPK_UNIT_ID]);
    snprintf(text, sizeof(text), "src/%u@%u", u, s->version[u]);
    ptf_root(VCS_CPK_SOURCE_CLOSURE, text, k->roots[VCS_CPK_SOURCE_CLOSURE]);
    ptf_root(VCS_CPK_KIND, ptm_class(u) == VCS_PROOF_ACTION_BUILD ?
                               "compile" : "test",
             k->roots[VCS_CPK_KIND]);
    if (u < PTM_HEADER_UNITS) {
        snprintf(text, sizeof(text), "shared.h@%u", s->header);
        ptf_root(VCS_CPK_DEPENDENCY_CLOSURE, text,
                 k->roots[VCS_CPK_DEPENDENCY_CLOSURE]);
    }
}

static struct ptf_spec ptm_spec(struct ptm *s, uint32_t u, bool pass)
{
    struct ptf_spec spec = pass ? ptf_pass() : ptf_fail();
    spec.action_class = ptm_class(u);
    spec.created = 1790000000u + s->candidate;
    return spec;
}

/* Two of three issuers execute the obligation independently. */
static bool ptm_execute(struct ptm *s, uint32_t u,
                        const struct vcs_component_proof_key_v1 *k)
{
    for (int i = 0; i < 2; i++) {
        int issuer = (int)(s->turn++ % 3u);
        if (!ptf_emit(&s->f, issuer, k, ptm_spec(s, u, ptm_truly_passes(s, u)),
                      NULL, NULL))
            return false;
    }
    return true;
}

static void ptm_audit(struct ptm *s, uint32_t u,
                      const struct vcs_proof_ticket_class *cls,
                      const struct vcs_proof_reuse_decision *d)
{
    s->tickets_classified += d->tickets_seen;
    for (uint32_t i = 0; i < d->tickets_seen; i++) {
        const char *why = cls[i].reason;
        if (cls[i].eligible) { s->eligible_proofs++; continue; }
        if (strcmp(why, VCS_PROOF_TICKET_SIGNER_IN_DOMAIN) == 0)
            s->why[PTM_WHY_DOMAIN]++;
        else if (strcmp(why, VCS_PROOF_TICKET_REUSED) == 0)
            s->why[PTM_WHY_REUSED]++;
        else if (strcmp(why, VCS_PROOF_TICKET_NOT_CHECKPOINTED) == 0)
            s->why[PTM_WHY_UNCOVERED]++;
        else
            s->why[PTM_WHY_OTHER]++;
    }
    if (d->false_hit_refused) s->why[PTM_WHY_ARTIFACT]++;
    if (d->outcome == VCS_PROOF_REUSE_HIT_PASS &&
        (!ptm_truly_passes(s, u) || d->distinct_pass_signers < 2))
        s->false_hits++;
}

/* On a hit, issuer C records that it reused the observation: provenance
 * that must never count toward a quorum. */
static bool ptm_provenance(struct ptm *s, uint32_t u,
                           const struct vcs_component_proof_key_v1 *k,
                           const struct vcs_proof_reuse_decision *d)
{
    if (u % 10u != 0 || d->used_count == 0) return true;
    struct ptf_spec spec = ptm_spec(s, u, true);
    spec.basis = VCS_PROOF_BASIS_REUSED;
    spec.basis_ref = d->used[0];
    return ptf_emit(&s->f, PTF_C, k, spec, NULL, NULL);
}

static bool ptm_account(struct ptm *s, uint32_t u,
                        const struct vcs_component_proof_key_v1 *k,
                        const struct vcs_proof_reuse_decision *d)
{
    bool reused = d->outcome == VCS_PROOF_REUSE_HIT_PASS ||
                  d->outcome == VCS_PROOF_REUSE_HIT_FAIL;
    bool unchanged = s->candidate > 0 && !s->changed[u] &&
                     !(s->candidate == 3 && u < PTM_HEADER_UNITS);
    s->unchanged += unchanged ? 1u : 0u;
    s->unchanged_reused += unchanged && reused ? 1u : 0u;
    if (d->outcome == VCS_PROOF_REUSE_REFUSE) s->refused++;
    if (reused) {
        s->reused++;
        return d->outcome != VCS_PROOF_REUSE_HIT_PASS ||
               ptm_provenance(s, u, k, d);
    }
    s->fresh++;
    return ptm_execute(s, u, k);
}

static bool ptm_obligation(struct ptm *s, uint32_t u)
{
    struct vcs_component_proof_key_v1 k;
    ptm_key(s, u, &k);
    struct vcs_proof_ticket_class cls[PTM_CAP];
    struct vcs_proof_reuse_decision d;
    s->f.tamper = s->candidate == 3 && u == PTM_TAMPER_UNIT;
    int64_t t0 = platform_time_monotonic_us();
    bool ok = ptf_decide(&s->f, &k, ptm_class(u), NULL, cls, PTM_CAP, &d);
    s->decide_us += (uint64_t)(platform_time_monotonic_us() - t0);
    if (s->f.tamper && d.outcome == VCS_PROOF_REUSE_HIT_PASS) ok = false;
    s->f.tamper = false;
    if (!ok) return false;
    s->obligations++;
    ptm_audit(s, u, cls, &d);
    return ptm_account(s, u, &k, &d);
}

/* Candidate-domain keys "prove" what the candidate touched. */
static bool ptm_attack(struct ptm *s, uint32_t u)
{
    struct vcs_component_proof_key_v1 k;
    ptm_key(s, u, &k);
    return ptf_emit(&s->f, PTF_AUTHOR, &k, ptm_spec(s, u, true), NULL, NULL) &&
           ptf_emit(&s->f, PTF_LOCAL, &k, ptm_spec(s, u, true), NULL, NULL);
}

static bool ptm_sync_all(struct ptm *s)
{
    static const int who[5] = {PTF_A, PTF_B, PTF_C, PTF_AUTHOR, PTF_LOCAL};
    bool ok = true;
    for (int i = 0; i < 5 && ok; i++) {
        struct vcs_proof_sync_report rep;
        uint64_t n = vcs_proof_issuer_log_count(s->f.logs[who[i]]);
        if (n == vcs_proof_receiver_issuer_leaves(s->f.rx, s->f.pub[who[i]]))
            continue;
        s->full_log_bytes += VCS_PROOF_CHECKPOINT_WIRE_BYTES +
                             n * VCS_PROOF_TICKET_WIRE_BYTES;
        ok = ptf_sync(&s->f, who[i], 1790000001u + s->candidate, &rep) &&
             rep.outcome == VCS_PROOF_SYNC_ADVANCED;
    }
    return ok;
}

/* The flaky issuer C reports FAIL for an obligation that truly passes. */
static bool ptm_contradict(struct ptm *s)
{
    struct vcs_component_proof_key_v1 k;
    ptm_key(s, PTM_FLAKY_UNIT, &k);
    return ptf_emit(&s->f, PTF_C, &k, ptm_spec(s, PTM_FLAKY_UNIT, false),
                    NULL, NULL);
}

static bool ptm_change(struct ptm *s)
{
    static const uint32_t per_mille[PTM_CANDIDATES] = {0, 10, 30, 50, 70, 100};
    memset(s->changed, 0, sizeof(s->changed));
    if (s->candidate == 3) s->header++; /* one shared header edit */
    uint32_t n = PTM_UNITS * per_mille[s->candidate] / 1000u;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t u = ptm_rand(s) % PTM_UNITS;
        if (u == PTM_TAMPER_UNIT || u == PTM_FLAKY_UNIT) continue;
        s->version[u]++;
        s->changed[u] = true;
        if (!ptm_attack(s, u)) return false;
    }
    if (s->candidate == 2 && (s->version[PTM_BROKEN_UNIT] & 1u) == 0) {
        s->version[PTM_BROKEN_UNIT]++;
        s->changed[PTM_BROKEN_UNIT] = true;
    }
    return true;
}

static bool ptm_candidate(struct ptm *s)
{
    if (!ptm_change(s)) return false;
    if (s->candidate == 4 && !ptm_contradict(s)) return false;
    if (!ptm_sync_all(s)) return false;
    for (uint32_t u = 0; u < PTM_UNITS; u++)
        if (!ptm_obligation(s, u)) return false;
    return ptm_sync_all(s);
}

static void ptm_print(const struct ptm *s)
{
    uint64_t cps = s->f.sync_checkpoints ? s->f.sync_checkpoints : 1u;
    uint64_t seen = s->tickets_classified ? s->tickets_classified : 1u;
    printf("\nproof_ticket_reuse_measure candidates=%u issuers=3 "
           "obligations_per_candidate=%u obligations=%llu eligible_proofs=%llu "
           "reused_proofs=%llu fresh_proofs=%llu refused=%llu "
           "unchanged=%llu reuse_rate_unchanged_pct=%.2f "
           "verify_wall_us_total=%llu verify_us_per_ticket=%.2f "
           "verify_us_per_checkpoint=%.1f bytes_synced=%llu "
           "bytes_full_logs=%llu fixture_wall_us=%llu "
           "fixture_process_cpu_us=%llu process_peak_rss_kib=%llu "
           "io_observed=%u io_read_ops=%llu io_write_ops=%llu "
           "false_hit_refusals=",
           PTM_CANDIDATES, PTM_UNITS, (unsigned long long)s->obligations,
           (unsigned long long)s->eligible_proofs,
           (unsigned long long)s->reused, (unsigned long long)s->fresh,
           (unsigned long long)s->refused, (unsigned long long)s->unchanged,
           s->unchanged ? 100.0 * (double)s->unchanged_reused /
                              (double)s->unchanged : 0.0,
           (unsigned long long)(s->decide_us + s->f.sync_verify_us),
           (double)s->decide_us / (double)seen,
           (double)s->f.sync_verify_us / (double)cps,
           (unsigned long long)s->f.sync_bytes,
           (unsigned long long)s->full_log_bytes,
           (unsigned long long)s->fixture_wall_us,
           (unsigned long long)s->fixture_process_cpu_us,
           (unsigned long long)s->process_peak_rss_kib,
           s->io_observed ? 1u : 0u,
           (unsigned long long)s->io_read_ops,
           (unsigned long long)s->io_write_ops);
    for (int i = 0; i < PTM_WHY_COUNT; i++)
        printf("%s%s:%llu", i ? "," : "", ptm_why_names[i],
               (unsigned long long)s->why[i]);
    printf(" false_hits=%llu\n", (unsigned long long)s->false_hits);
}

static int ptm_case_measure(void)
{
    int failures = 0;
    TEST_CASE("proof_ticket: 6 candidates x 3 issuers x 200 obligations") {
        struct ptm *s = calloc(1, sizeof(*s));
        ASSERT(s != NULL);
        int64_t wall_before = platform_time_monotonic_us();
        clock_t cpu_before = clock();
#if !defined(_WIN32)
        struct rusage usage_before, usage_after;
        bool usage_started = getrusage(RUSAGE_SELF, &usage_before) == 0;
#endif
        bool ok = ptf_init(&s->f);
        s->rng = 0x5eed1234abcdull;
        for (s->candidate = 0; ok && s->candidate < PTM_CANDIDATES;
             s->candidate++)
            ok = ptm_candidate(s);
        s->fixture_wall_us = (uint64_t)(platform_time_monotonic_us() -
                                        wall_before);
        clock_t cpu_after = clock();
        if (cpu_before != (clock_t)-1 && cpu_after != (clock_t)-1 &&
            cpu_after >= cpu_before)
            s->fixture_process_cpu_us = (uint64_t)(
                (double)(cpu_after - cpu_before) * 1000000.0 /
                (double)CLOCKS_PER_SEC);
#if !defined(_WIN32)
        if (usage_started && getrusage(RUSAGE_SELF, &usage_after) == 0) {
            s->io_observed = true;
#if defined(__APPLE__)
            s->process_peak_rss_kib = (uint64_t)usage_after.ru_maxrss / 1024u;
#else
            s->process_peak_rss_kib = (uint64_t)usage_after.ru_maxrss;
#endif
            s->io_read_ops = (uint64_t)(usage_after.ru_inblock -
                                        usage_before.ru_inblock);
            s->io_write_ops = (uint64_t)(usage_after.ru_oublock -
                                         usage_before.ru_oublock);
        }
#endif
        ptm_print(s);
        struct ptm r = *s;
        ptf_free(&s->f);
        free(s);
        ASSERT(ok);
        ASSERT_EQ(r.obligations, (uint64_t)PTM_UNITS * PTM_CANDIDATES);
        ASSERT_EQ(r.false_hits, 0u);
        ASSERT(r.why[PTM_WHY_ARTIFACT] == 1u);   /* the tampered artifact */
        ASSERT(r.why[PTM_WHY_DOMAIN] > 0u);      /* attacks were seen */
        ASSERT(r.why[PTM_WHY_REUSED] > 0u);      /* provenance never counts */
        ASSERT(r.refused >= 1u);                  /* the contradiction */
        ASSERT(r.unchanged_reused * 100u > r.unchanged * 95u);
        ASSERT(r.f.sync_bytes < r.full_log_bytes);
    } TEST_END
    return failures;
}

/* Optional same-issuer checkpoint density probe. One covered ticket is
 * followed by signed, same-leaf descendants, so it measures checkpoint
 * bookkeeping without changing the ticket corpus or receiver policy. */
static int ptm_case_checkpoint_density(void)
{
    if (!getenv("Z23_PROOF_CP_SCAN_BENCH")) return 0;
    int failures = 0;
    TEST_CASE("proof_ticket: checkpoint density measurement") {
        static const size_t sizes[] = {128u, 512u, 2048u};
        for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
            struct ptf *f = calloc(1, sizeof(*f));
            ASSERT(f != NULL);
            ASSERT(ptf_init(f));
            ASSERT(ptf_emit(f, PTF_A, &f->base, ptf_pass(), NULL, NULL));
            int64_t wall_before = platform_time_monotonic_us();
            clock_t cpu_before = clock();
#if !defined(_WIN32)
            struct rusage usage_before, usage_after;
            bool usage_started = getrusage(RUSAGE_SELF, &usage_before) == 0;
#endif
            bool ok = true;
            for (size_t i = 0; ok && i < sizes[k]; i++) {
                struct vcs_proof_sync_report rep;
                ok = ptf_sync(f, PTF_A, 1790000001u + i, &rep) &&
                     rep.outcome == VCS_PROOF_SYNC_ADVANCED;
            }
            uint64_t wall_us = (uint64_t)(platform_time_monotonic_us() -
                                          wall_before);
            clock_t cpu_after = clock();
            uint64_t cpu_us = 0, rss_kib = 0, read_ops = 0, write_ops = 0;
            if (cpu_before != (clock_t)-1 && cpu_after != (clock_t)-1 &&
                cpu_after >= cpu_before)
                cpu_us = (uint64_t)((double)(cpu_after - cpu_before) *
                                    1000000.0 / (double)CLOCKS_PER_SEC);
#if !defined(_WIN32)
            if (usage_started && getrusage(RUSAGE_SELF, &usage_after) == 0) {
#if defined(__APPLE__)
                rss_kib = (uint64_t)usage_after.ru_maxrss / 1024u;
#else
                rss_kib = (uint64_t)usage_after.ru_maxrss;
#endif
                read_ops = (uint64_t)(usage_after.ru_inblock -
                                      usage_before.ru_inblock);
                write_ops = (uint64_t)(usage_after.ru_oublock -
                                       usage_before.ru_oublock);
            }
#endif
            printf("\nproof_checkpoint_density checkpoints=%zu tickets=1 "
                   "wall_us=%llu cpu_us=%llu process_peak_rss_kib=%llu "
                   "io_read_ops=%llu io_write_ops=%llu\n", sizes[k],
                   (unsigned long long)wall_us,
                   (unsigned long long)cpu_us,
                   (unsigned long long)rss_kib,
                   (unsigned long long)read_ops,
                   (unsigned long long)write_ops);
            ASSERT(ok);
            ASSERT_EQ(vcs_proof_receiver_issuer_checkpoints(f->rx,
                                                             f->pub[PTF_A]),
                      sizes[k]);
            ptf_free(f);
            free(f);
        }
    } TEST_END
    return failures;
}

/* Optional CAS reconstruction probe. The store grows in place; each timed
 * rebuild replays its entire current inventory and checks the old receiver
 * before publishing the new projection. Store population is outside time. */
static int ptm_case_rebuild_density(void)
{
    if (!getenv("Z23_PROOF_REBUILD_DENSITY_BENCH")) return 0;
    int failures = 0;
    TEST_CASE("proof_ticket: CAS checkpoint rebuild density measurement") {
        static const size_t sizes[] = {128u, 512u, 2048u};
        struct ptf *f = calloc(1, sizeof(*f));
        ASSERT(f != NULL);
        ASSERT(ptf_init(f));
        char dir[256];
        test_make_tmpdir(dir, sizeof(dir), "proof_ticket", "rebuilddensity");
        struct vcs_package_store *store =
            vcs_package_store_open(dir, UINT64_C(128) * 1024 * 1024);
        ASSERT(store != NULL);
        uint8_t ticket[VCS_PROOF_TICKET_WIRE_BYTES], root[32];
        ASSERT(ptf_emit(f, PTF_A, &f->base, ptf_pass(), ticket, NULL));
        ASSERT(vcs_proof_ticket_store_put(store, ticket, sizeof(ticket), root));
        size_t produced = 0;
        for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
            uint8_t cp[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
            bool populated = true;
            while (populated && produced < sizes[k]) {
                populated = vcs_proof_issuer_log_checkpoint(
                                f->logs[PTF_A], 1790000001u + produced,
                                cp) &&
                            vcs_proof_ticket_store_put(store, cp, sizeof(cp),
                                                       root);
                produced++;
            }
            ASSERT(populated);
            int64_t wall_before = platform_time_monotonic_us();
            clock_t cpu_before = clock();
#if !defined(_WIN32)
            struct rusage usage_before, usage_after;
            bool usage_started = getrusage(RUSAGE_SELF, &usage_before) == 0;
#endif
            size_t tickets = 0, checkpoints = 0, skipped = 0;
            bool rebuilt = vcs_proof_receiver_rebuild(
                f->rx, store, &tickets, &checkpoints, &skipped);
            uint64_t wall_us = (uint64_t)(platform_time_monotonic_us() -
                                          wall_before);
            clock_t cpu_after = clock();
            uint64_t cpu_us = 0, rss_kib = 0, read_ops = 0, write_ops = 0;
            if (cpu_before != (clock_t)-1 && cpu_after != (clock_t)-1 &&
                cpu_after >= cpu_before)
                cpu_us = (uint64_t)((double)(cpu_after - cpu_before) *
                                    1000000.0 / (double)CLOCKS_PER_SEC);
#if !defined(_WIN32)
            if (usage_started && getrusage(RUSAGE_SELF, &usage_after) == 0) {
#if defined(__APPLE__)
                rss_kib = (uint64_t)usage_after.ru_maxrss / 1024u;
#else
                rss_kib = (uint64_t)usage_after.ru_maxrss;
#endif
                read_ops = (uint64_t)(usage_after.ru_inblock -
                                      usage_before.ru_inblock);
                write_ops = (uint64_t)(usage_after.ru_oublock -
                                       usage_before.ru_oublock);
            }
#endif
            printf("\nproof_rebuild_density listed=%zu tickets=%zu "
                   "checkpoints=%zu skipped=%zu success=%u wall_us=%llu "
                   "cpu_us=%llu process_peak_rss_kib=%llu "
                   "io_read_ops=%llu io_write_ops=%llu\n",
                   sizes[k] + 1u, tickets, checkpoints, skipped,
                   rebuilt ? 1u : 0u, (unsigned long long)wall_us,
                   (unsigned long long)cpu_us,
                   (unsigned long long)rss_kib,
                   (unsigned long long)read_ops,
                   (unsigned long long)write_ops);
            ASSERT(rebuilt);
            ASSERT_EQ(tickets, (size_t)1);
            ASSERT_EQ(checkpoints, sizes[k]);
            ASSERT_EQ(vcs_proof_receiver_issuer_leaves(f->rx, f->pub[PTF_A]),
                      (uint64_t)1);
        }
        vcs_package_store_close(store);
        test_rm_rf(dir);
        ptf_free(f);
        free(f);
    } TEST_END
    return failures;
}

/* Optional receiver-only boundary probe. The issuer log and receiver are
 * in memory; this isolates index and signed-delta handling from the package
 * store's separate tracked-object enumeration bound. */
static int ptm_case_receiver_boundary(void)
{
    if (!getenv("Z23_PROOF_RECEIVER_BOUNDARY_BENCH")) return 0;
    int failures = 0;
    TEST_CASE("proof_ticket: receiver sync crosses 4096 tickets") {
        static const size_t sizes[] = {4095u, 4096u, 4097u};
        struct ptf *f = calloc(1, sizeof(*f));
        ASSERT(f != NULL);
        ASSERT(ptf_init(f));
        size_t emitted = 0;
        for (size_t k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
            bool ready = true;
            while (ready && emitted < sizes[k]) {
                ready = ptf_emit(f, PTF_A, &f->base, ptf_pass(), NULL, NULL);
                emitted++;
            }
            ASSERT(ready);
            int64_t wall_before = platform_time_monotonic_us();
            clock_t cpu_before = clock();
#if !defined(_WIN32)
            struct rusage usage_before, usage_after;
            bool usage_started = getrusage(RUSAGE_SELF, &usage_before) == 0;
#endif
            struct vcs_proof_sync_report rep = {0};
            bool synced = ptf_sync(f, PTF_A, 1790000001u + k, &rep);
            uint64_t wall_us = (uint64_t)(platform_time_monotonic_us() -
                                          wall_before);
            clock_t cpu_after = clock();
            uint64_t cpu_us = 0, rss_kib = 0, read_ops = 0, write_ops = 0;
            if (cpu_before != (clock_t)-1 && cpu_after != (clock_t)-1 &&
                cpu_after >= cpu_before)
                cpu_us = (uint64_t)((double)(cpu_after - cpu_before) *
                                    1000000.0 / (double)CLOCKS_PER_SEC);
#if !defined(_WIN32)
            if (usage_started && getrusage(RUSAGE_SELF, &usage_after) == 0) {
#if defined(__APPLE__)
                rss_kib = (uint64_t)usage_after.ru_maxrss / 1024u;
#else
                rss_kib = (uint64_t)usage_after.ru_maxrss;
#endif
                read_ops = (uint64_t)(usage_after.ru_inblock -
                                      usage_before.ru_inblock);
                write_ops = (uint64_t)(usage_after.ru_oublock -
                                       usage_before.ru_oublock);
            }
#endif
            printf("\nproof_receiver_boundary tickets=%zu delta=%llu "
                   "bytes_synced=%llu wall_us=%llu cpu_us=%llu "
                   "process_peak_rss_kib=%llu io_read_ops=%llu "
                   "io_write_ops=%llu\n", sizes[k],
                   (unsigned long long)(rep.leaves_after - rep.leaves_before),
                   (unsigned long long)rep.bytes,
                   (unsigned long long)wall_us,
                   (unsigned long long)cpu_us,
                   (unsigned long long)rss_kib,
                   (unsigned long long)read_ops,
                   (unsigned long long)write_ops);
            ASSERT(synced);
            ASSERT_EQ(rep.outcome, VCS_PROOF_SYNC_ADVANCED);
            ASSERT_EQ(vcs_proof_receiver_ticket_count(f->rx), sizes[k]);
            ASSERT_EQ(vcs_proof_receiver_issuer_leaves(f->rx, f->pub[PTF_A]),
                      (uint64_t)sizes[k]);
        }
        ptf_free(f);
        free(f);
    } TEST_END
    return failures;
}

/* ── per-change admission ───────────────────────────────────────────── */

enum { PTA_A0, PTA_A1, PTA_ATEST, PTA_B0, PTA_B1, PTA_BUNIT, PTA_BLINK,
       PTA_C0, PTA_CUNIT, PTA_CLINK, PTA_D0, PTA_DUNIT, PTA_N };

struct pta_row {
    const char *name;
    const char *component;
    enum vcs_proof_action_class cls;
    bool caller;        /* integration edge to libA */
    bool links_callee;  /* executes libA bytes */
};

static const struct pta_row pta_rows[PTA_N] = {
    {"libA/a0.o", "libA", VCS_PROOF_ACTION_BUILD, false, false},
    {"libA/a1.o", "libA", VCS_PROOF_ACTION_BUILD, false, false},
    {"libA/test", "libA", VCS_PROOF_ACTION_CHECK, false, true},
    {"libB/b0.o", "libB", VCS_PROOF_ACTION_BUILD, true, false},
    {"libB/b1.o", "libB", VCS_PROOF_ACTION_BUILD, true, false},
    {"libB/unit", "libB", VCS_PROOF_ACTION_CHECK, true, false},
    {"libB/linked", "libB", VCS_PROOF_ACTION_CHECK, true, true},
    {"libC/c0.o", "libC", VCS_PROOF_ACTION_BUILD, true, false},
    {"libC/unit", "libC", VCS_PROOF_ACTION_CHECK, true, false},
    {"libC/linked", "libC", VCS_PROOF_ACTION_CHECK, true, true},
    {"libD/d0.o", "libD", VCS_PROOF_ACTION_BUILD, false, false},
    {"libD/unit", "libD", VCS_PROOF_ACTION_CHECK, false, false},
};

struct pta {
    struct ptf f;
    uint32_t a0_version;       /* private implementation of libA */
    uint32_t header_version;   /* libA public header */
    struct vcs_component_proof_key_v1 keys[PTA_N];
    struct vcs_proof_obligation obs[PTA_N];
    struct vcs_proof_admission_result res[PTA_N];
    struct vcs_proof_admission_report rep;
    struct vcs_proof_change change;
};

static bool pta_contract(uint32_t header_version, uint8_t out[32])
{
    char tok[32];
    snprintf(tok, sizeof(tok), "a_api_v%u", header_version);
    const char *tokens[3] = {"int", tok, "(int);"};
    const char *syms[1] = {"a_api:i->i"};
    struct vcs_component_contract c = {"libA", tokens, 3, syms, 1, NULL, 0};
    return vcs_component_contract_root(&c, out);
}

static bool pta_key(struct pta *p, int i)
{
    const struct pta_row *row = &pta_rows[i];
    struct vcs_component_proof_key_v1 *k = &p->keys[i];
    char text[96];
    *k = p->f.base;
    ptf_root(VCS_CPK_UNIT_ID, row->name, k->roots[VCS_CPK_UNIT_ID]);
    uint32_t v = i == PTA_A0 ? p->a0_version : 0u;
    snprintf(text, sizeof(text), "%s@%u", row->name, v);
    ptf_root(VCS_CPK_SOURCE_CLOSURE, text, k->roots[VCS_CPK_SOURCE_CLOSURE]);
    if (row->links_callee) {
        snprintf(text, sizeof(text), "libA-impl@%u.%u", p->a0_version,
                 p->header_version);
        ptf_root(VCS_CPK_DEPENDENCY_CLOSURE, text,
                 k->roots[VCS_CPK_DEPENDENCY_CLOSURE]);
    }
    if (strcmp(row->component, "libA") == 0 && row->cls == VCS_PROOF_ACTION_BUILD) {
        snprintf(text, sizeof(text), "a.h@%u", p->header_version);
        ptf_root(VCS_CPK_DEPENDENCY_CLOSURE, text,
                 k->roots[VCS_CPK_DEPENDENCY_CLOSURE]);
    }
    if (!row->caller) return true;
    struct vcs_component_edge edge = {"libA", {0}};
    return pta_contract(p->header_version, edge.contract_root) &&
           vcs_component_integration_edges_root(
               &edge, 1, k->roots[VCS_CPK_INTEGRATION_EDGES]);
}

static bool pta_build(struct pta *p, bool scope_known, uint32_t before_header)
{
    for (int i = 0; i < PTA_N; i++) {
        if (!pta_key(p, i)) return false;
        p->obs[i] = (struct vcs_proof_obligation){
            pta_rows[i].name, pta_rows[i].component, pta_rows[i].cls,
            &p->keys[i], pta_rows[i].caller,
            strcmp(pta_rows[i].component, "libD") != 0};
    }
    p->change.component_id = "libA";
    p->change.scope_known = scope_known;
    return pta_contract(before_header, p->change.contract_root_before) &&
           pta_contract(p->header_version, p->change.contract_root_after);
}

/* Run fresh obligations on issuers A and B and sync both logs. */
static bool pta_settle(struct pta *p)
{
    for (int i = 0; i < PTA_N; i++) {
        if (p->res[i].status != VCS_PROOF_ADMIT_FRESH) continue;
        struct ptf_spec spec = ptf_pass();
        spec.action_class = pta_rows[i].cls;
        if (!ptf_emit(&p->f, PTF_A, &p->keys[i], spec, NULL, NULL) ||
            !ptf_emit(&p->f, PTF_B, &p->keys[i], spec, NULL, NULL))
            return false;
    }
    struct vcs_proof_sync_report rep;
    return ptf_sync(&p->f, PTF_A, 0, &rep) && ptf_sync(&p->f, PTF_B, 0, &rep);
}

static bool pta_admit(struct pta *p, const char *label)
{
    struct vcs_proof_admission_context ctx = ptf_context(&p->f);
    char line[512];
    if (!vcs_proof_admission_run(&ctx, &p->change, p->obs, PTA_N, p->res,
                                 &p->rep) ||
        !vcs_proof_admission_report_line(&p->change, &p->rep, line,
                                         sizeof(line)))
        return false;
    printf("\nproof_admission scenario=%s %s\n  caller proofs still valid:",
           label, line);
    for (int i = 0; i < PTA_N; i++)
        if (pta_rows[i].caller && p->res[i].status == VCS_PROOF_ADMIT_REUSED)
            printf(" %s", pta_rows[i].name);
    printf("\n");
    return true;
}

static bool pta_fresh(const struct pta *p, int i)
{
    return p->res[i].status == VCS_PROOF_ADMIT_FRESH;
}

static int pta_case_private_edit(struct pta *p)
{
    int failures = 0;
    TEST_CASE("proof_admission: private edit keeps caller proofs valid") {
        p->a0_version++;
        ASSERT(pta_build(p, true, p->header_version));
        ASSERT(pta_admit(p, "private-edit"));
        ASSERT(pta_fresh(p, PTA_A0) && !pta_fresh(p, PTA_A1));
        ASSERT(pta_fresh(p, PTA_ATEST));
        ASSERT(!pta_fresh(p, PTA_B0) && !pta_fresh(p, PTA_B1));
        ASSERT(!pta_fresh(p, PTA_BUNIT) && !pta_fresh(p, PTA_CUNIT));
        ASSERT(pta_fresh(p, PTA_BLINK) && pta_fresh(p, PTA_CLINK));
        ASSERT(!pta_fresh(p, PTA_D0) && !pta_fresh(p, PTA_DUNIT));
        ASSERT_EQ(p->rep.proofs_fresh, 4u);
        ASSERT_STR_EQ(p->rep.fallback_reason, VCS_PROOF_FALLBACK_NONE);
        ASSERT(pta_settle(p));
    } TEST_END
    return failures;
}

static int pta_case_header_edit(struct pta *p)
{
    int failures = 0;
    TEST_CASE("proof_admission: header/ABI edit invalidates exactly dependents") {
        uint32_t before = p->header_version++;
        ASSERT(pta_build(p, true, before));
        ASSERT(pta_admit(p, "header-edit"));
        for (int i = 0; i < PTA_N; i++) {
            bool dependent = strcmp(pta_rows[i].component, "libD") != 0;
            ASSERT_EQ(pta_fresh(p, i), dependent);
        }
        ASSERT_EQ(p->rep.integration_edges_rerun, 7u);
        ASSERT_STR_EQ(p->rep.fallback_reason, VCS_PROOF_FALLBACK_DEPENDENCY);
        ASSERT(pta_settle(p));
    } TEST_END
    return failures;
}

static int pta_case_unknown_scope(struct pta *p)
{
    int failures = 0;
    TEST_CASE("proof_admission: unknown scope runs the whole reach fresh") {
        ASSERT(pta_build(p, false, p->header_version));
        ASSERT(pta_admit(p, "unknown-scope"));
        for (int i = 0; i < PTA_N; i++)
            ASSERT_EQ(pta_fresh(p, i), p->obs[i].in_reach);
        ASSERT_STR_EQ(p->rep.fallback_reason,
                      VCS_PROOF_FALLBACK_UNKNOWN_SCOPE);
    } TEST_END
    return failures;
}

static int pta_case_conflict(struct pta *p)
{
    int failures = 0;
    TEST_CASE("proof_admission: a contradiction falls back to conflict") {
        ASSERT(pta_build(p, true, p->header_version));
        struct ptf_spec fail = ptf_fail();
        struct vcs_proof_sync_report rep;
        ASSERT(ptf_emit(&p->f, PTF_C, &p->keys[PTA_BUNIT], fail, NULL, NULL));
        ASSERT(ptf_sync(&p->f, PTF_C, 0, &rep));
        ASSERT(pta_admit(p, "conflict"));
        ASSERT(pta_fresh(p, PTA_BUNIT));
        ASSERT_STR_EQ(p->res[PTA_BUNIT].reason, VCS_PROOF_OBSERVATION_CONFLICT);
        ASSERT_STR_EQ(p->rep.fallback_reason, VCS_PROOF_FALLBACK_CONFLICT);
    } TEST_END
    return failures;
}

static int pta_case_refused_policy(struct pta *p)
{
    int failures = 0;
    TEST_CASE("proof_admission: refused reuse never reports no fallback") {
        ASSERT(pta_build(p, true, p->header_version));
        struct vcs_proof_admission_context ctx = ptf_context(&p->f);
        ctx.policy = NULL;
        struct vcs_proof_admission_result result;
        struct vcs_proof_admission_report report;
        ASSERT(vcs_proof_admission_run(&ctx, &p->change,
                                      &p->obs[PTA_BUNIT], 1u,
                                      &result, &report));
        ASSERT_EQ(result.decision.outcome, VCS_PROOF_REUSE_REFUSE);
        ASSERT_EQ(result.status, VCS_PROOF_ADMIT_FRESH);
        ASSERT_STR_EQ(result.reason, VCS_PROOF_REUSE_WHY_ARGUMENTS);
        ASSERT_STR_EQ(report.fallback_reason, VCS_PROOF_FALLBACK_POLICY);
    } TEST_END
    return failures;
}

static int pta_cases(void)
{
    int failures = 0;
    struct pta *p = calloc(1, sizeof(*p));
    if (!p || !ptf_init(&p->f)) {
        printf("proof_admission: fixture... FAIL (allocation)\n");
        free(p);
        return 1;
    }
    /* Baseline: every obligation proven by A and B. */
    bool ok = pta_build(p, true, 0);
    for (int i = 0; ok && i < PTA_N; i++)
        p->res[i].status = VCS_PROOF_ADMIT_FRESH;
    ok = ok && pta_settle(p);
    failures += ok ? 0 : 1;
    if (ok) {
        failures += pta_case_private_edit(p);
        failures += pta_case_header_edit(p);
        failures += pta_case_unknown_scope(p);
        failures += pta_case_conflict(p);
        failures += pta_case_refused_policy(p);
    }
    ptf_free(&p->f);
    free(p);
    return failures;
}

int test_proof_ticket_measure(void);

int test_proof_ticket_measure(void)
{
    int failures = 0;
    failures += ptm_case_measure();
    failures += ptm_case_checkpoint_density();
    failures += ptm_case_rebuild_density();
    failures += ptm_case_receiver_boundary();
    failures += pta_cases();
    return failures;
}
