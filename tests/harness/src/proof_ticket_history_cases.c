/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Restart-anywhere receiver history for the proof_ticket_reuse
 *          group: revocation after a reopened rebuild, a fork visible only
 *          past the first catalog page, and a process that dies inside a
 *          rebuild. Every case keeps HIT, MISS and REFUSE distinct. */

#include "test/test_core.h"

#include "test/proof_ticket_fixture.h"
#include "vcs/blob_store.h"
#include "vcs/package_store.h"
#include "vcs/proof_ticket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#define PHC_QUOTA (UINT64_C(64) * 1024 * 1024)
#define PHC_CAP 16u
#define PHC_CRASHED 42
#define PHC_FILLER (VCS_PACKAGE_STORE_PAGE_MAX + 4u)

static struct ptf g_h;

static bool phc_fresh(void)
{
    ptf_free(&g_h);
    return ptf_init(&g_h);
}

static void phc_key(const char *unit, struct vcs_component_proof_key_v1 *k)
{
    *k = g_h.base;
    ptf_root(VCS_CPK_UNIT_ID, unit, k->roots[VCS_CPK_UNIT_ID]);
}

static bool phc_put(struct vcs_package_store *store, const uint8_t *wire,
                    size_t len, uint8_t root[32])
{
    uint8_t local[32];
    return vcs_proof_ticket_store_put(store, wire, len, root ? root : local);
}

/* Checkpoint one signer's whole log and store its tickets and checkpoint.
 * The exact checkpoint wire is returned for a live sync of the same view. */
static bool phc_store_log(struct vcs_package_store *store, int signer,
                          const uint8_t (*wires)[VCS_PROOF_TICKET_WIRE_BYTES],
                          size_t count, uint64_t created,
                          uint8_t cp[VCS_PROOF_CHECKPOINT_WIRE_BYTES])
{
    uint8_t local[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
    uint8_t *out = cp ? cp : local;
    for (size_t i = 0; i < count; i++)
        if (!phc_put(store, wires[i], VCS_PROOF_TICKET_WIRE_BYTES, NULL))
            return false;
    return vcs_proof_issuer_log_checkpoint(g_h.logs[signer], created, out) &&
           phc_put(store, out, VCS_PROOF_CHECKPOINT_WIRE_BYTES, NULL);
}

/* Sync a receiver with exactly one stored ticket and checkpoint. */
static bool phc_sync_one_into(struct vcs_proof_receiver *rx, const uint8_t *cp,
                              const uint8_t *ticket)
{
    const uint8_t *delta[] = {ticket};
    const size_t lens[] = {VCS_PROOF_TICKET_WIRE_BYTES};
    struct vcs_proof_sync_report rep;
    return vcs_proof_receiver_sync(rx, cp, VCS_PROOF_CHECKPOINT_WIRE_BYTES,
                                   delta, lens, 1, &rep) &&
           rep.outcome == VCS_PROOF_SYNC_ADVANCED;
}

static bool phc_sync_one(const uint8_t *cp, const uint8_t *ticket)
{
    return phc_sync_one_into(g_h.rx, cp, ticket);
}

/* Filler packages: `count` of them, all sorting before `late` when `late`
 * is non-NULL, so `late` cannot appear on the first catalog page. */
static bool phc_fill(struct vcs_package_store *store, const uint8_t *late,
                     uint32_t count)
{
    uint32_t added = 0;
    for (uint32_t i = 0; added < count && i < 1000000u; i++) {
        uint8_t blob[8] = {'h', 'i', 's', 't', (uint8_t)i, (uint8_t)(i >> 8),
                           (uint8_t)(i >> 16), (uint8_t)(i >> 24)};
        uint8_t root[32];
        if (!vcs_blob_root(blob, sizeof(blob), root)) return false;
        if (late && memcmp(root, late, 32) >= 0) continue;
        if (!phc_put(store, blob, sizeof(blob), NULL)) return false;
        added++;
    }
    return added == count;
}

static struct vcs_proof_receiver *phc_rebuilt(struct vcs_package_store *store,
                                              size_t rows)
{
    struct vcs_proof_receiver *rx = vcs_proof_receiver_new();
    size_t tickets = 0, cps = 0, skipped = 0;
    if (rx && vcs_proof_receiver_rebuild_bounded(rx, store, rows, &tickets,
                                                 &cps, &skipped))
        return rx;
    vcs_proof_receiver_free(rx);
    return NULL;
}

static bool phc_decide(const struct vcs_proof_receiver *rx,
                       const struct vcs_component_proof_key_v1 *key,
                       const struct vcs_proof_reuse_policy *policy,
                       struct vcs_proof_ticket_class cls[PHC_CAP],
                       struct vcs_proof_reuse_decision *d)
{
    struct vcs_proof_reuse_request req = {
        .local = key, .action_class = VCS_PROOF_ACTION_CHECK,
        .domain = &g_h.domain, .policy = policy ? policy : &g_h.policy,
        .artifacts = &g_h.source,
    };
    return vcs_proof_reuse_decide(rx, &req, cls, PHC_CAP, d);
}

static const char *phc_reason_of(const struct vcs_proof_ticket_class *cls,
                                 uint32_t seen, int signer)
{
    for (uint32_t i = 0; i < seen; i++)
        if (memcmp(cls[i].producer_pubkey, g_h.pub[signer], 32) == 0)
            return cls[i].reason;
    return "absent";
}

/* ── a process that dies inside a multi-page rebuild ────────────────── */

#if !defined(_WIN32)
void vcs_proof_receiver_test_before_recheck(void (*hook)(void *),
                                            void *context);

static void phc_die(void *context)
{
    (void)context;
    _exit(PHC_CRASHED);
}

/* A child process scans every page, then dies before it can publish. */
static bool phc_crash_rebuild(const char *dir, size_t rows)
{
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        struct vcs_package_store *store = vcs_package_store_open(dir, PHC_QUOTA);
        if (!store) _exit(2);
        struct vcs_proof_receiver *rx = vcs_proof_receiver_new();
        if (!rx) _exit(3);
        vcs_proof_receiver_test_before_recheck(phc_die, NULL);
        size_t tickets = 0, cps = 0, skipped = 0;
        bool rebuilt = vcs_proof_receiver_rebuild_bounded(
            rx, store, rows, &tickets, &cps, &skipped);
        _exit(rebuilt ? 4 : 5);
    }
    int status = 0;
    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
           WEXITSTATUS(status) == PHC_CRASHED;
}
#endif

/* ── revocation, conflict and a crash after a reopened rebuild ──────── */

struct phc_keys {
    struct vcs_component_proof_key_v1 both, two, disputed, never;
};

/* After any restart, without revocation: HIT, REFUSE and MISS stay apart. */
static int phc_expect_unrevoked(const struct vcs_proof_receiver *rx,
                                const struct phc_keys *k)
{
    int failures = 0;
    TEST_CASE("proof_history: restarted receiver keeps HIT/MISS/REFUSE distinct") {
        struct vcs_proof_ticket_class cls[PHC_CAP];
        struct vcs_proof_reuse_decision d;
        ASSERT(phc_decide(rx, &k->both, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT_EQ(d.distinct_pass_signers, (uint32_t)3);
        ASSERT(phc_decide(rx, &k->two, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT(phc_decide(rx, &k->disputed, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_REFUSE);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_OBSERVATION_CONFLICT);
        ASSERT(phc_decide(rx, &k->never, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_NONE);
    } TEST_END
    return failures;
}

static int phc_case_revoked_after_reopen(void)
{
    int failures = 0;
    TEST_CASE("proof_history: revocation after reopen neither hits nor forgets") {
        ASSERT(phc_fresh());
        struct phc_keys k;
        phc_key("unit/both", &k.both);
        phc_key("unit/two", &k.two);
        phc_key("unit/disputed", &k.disputed);
        phc_key("unit/never", &k.never);
        uint8_t a[3][VCS_PROOF_TICKET_WIRE_BYTES];
        uint8_t b[2][VCS_PROOF_TICKET_WIRE_BYTES];
        uint8_t c[2][VCS_PROOF_TICKET_WIRE_BYTES];
        ASSERT(ptf_emit(&g_h, PTF_A, &k.both, ptf_pass(), a[0], NULL));
        ASSERT(ptf_emit(&g_h, PTF_A, &k.two, ptf_pass(), a[1], NULL));
        ASSERT(ptf_emit(&g_h, PTF_A, &k.disputed, ptf_pass(), a[2], NULL));
        ASSERT(ptf_emit(&g_h, PTF_B, &k.both, ptf_pass(), b[0], NULL));
        ASSERT(ptf_emit(&g_h, PTF_B, &k.two, ptf_pass(), b[1], NULL));
        ASSERT(ptf_emit(&g_h, PTF_C, &k.both, ptf_pass(), c[0], NULL));
        ASSERT(ptf_emit(&g_h, PTF_C, &k.disputed, ptf_fail(), c[1], NULL));
        char dir[256];
        test_make_tmpdir(dir, sizeof(dir), "proof_history", "revoked");
        struct vcs_package_store *store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        ASSERT(phc_fill(store, NULL, PHC_FILLER));
        ASSERT(phc_store_log(store, PTF_A, a, 3, 41, NULL));
        ASSERT(phc_store_log(store, PTF_B, b, 2, 42, NULL));
        ASSERT(phc_store_log(store, PTF_C, c, 2, 43, NULL));
        const size_t rows = PHC_FILLER + 10u;
        vcs_package_store_close(store);
        store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        struct vcs_proof_receiver *rx = phc_rebuilt(store, rows);
        ASSERT(rx != NULL);
        failures += phc_expect_unrevoked(rx, &k);

        /* A revoked: its rebuilt history cannot authorize, B and C still do. */
        struct vcs_proof_ticket_class cls[PHC_CAP];
        struct vcs_proof_reuse_decision d;
        struct vcs_proof_reuse_policy revoked = g_h.policy;
        revoked.revoked = (const uint8_t (*)[32])&g_h.pub[PTF_A];
        revoked.revoked_count = 1;
        ASSERT(phc_decide(rx, &k.both, &revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT_EQ(d.eligible_pass, (uint32_t)2);
        ASSERT_STR_EQ(phc_reason_of(cls, d.tickets_seen, PTF_A),
                      VCS_PROOF_TICKET_SIGNER_REVOKED);
        ASSERT(phc_decide(rx, &k.two, &revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_QUORUM);
        ASSERT_EQ(d.eligible_pass, (uint32_t)1);

        /* A and B revoked: nothing authorizes, still a MISS, not a REFUSE. */
        uint8_t ab[2][32];
        memcpy(ab[0], g_h.pub[PTF_A], 32);
        memcpy(ab[1], g_h.pub[PTF_B], 32);
        struct vcs_proof_reuse_policy both_revoked = g_h.policy;
        both_revoked.revoked = (const uint8_t (*)[32])ab;
        both_revoked.revoked_count = 2;
        ASSERT(phc_decide(rx, &k.both, &both_revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_EQ(d.eligible_pass, (uint32_t)1);
        ASSERT(phc_decide(rx, &k.two, &both_revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_INELIGIBLE);
        ASSERT_EQ(d.tickets_seen, (uint32_t)2);

        /* Revoking C, who never signed `two`, leaves `two` a HIT. */
        struct vcs_proof_reuse_policy c_revoked = g_h.policy;
        c_revoked.revoked = (const uint8_t (*)[32])&g_h.pub[PTF_C];
        c_revoked.revoked_count = 1;
        ASSERT(phc_decide(rx, &k.two, &c_revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);

        /* Policy is read per decision: the receiver forgot nothing. */
        ASSERT_EQ(vcs_proof_receiver_ticket_count(rx), (size_t)7);
        failures += phc_expect_unrevoked(rx, &k);

        /* A second restart rebuilds over the live receiver as its anchor and
         * reaches the same verdicts under the same revoked policy. */
        vcs_package_store_close(store);
        store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        size_t tickets = 0, cps = 0, skipped = 0;
        ASSERT(vcs_proof_receiver_rebuild_bounded(rx, store, rows, &tickets,
                                                  &cps, &skipped));
        ASSERT_EQ(tickets, (size_t)7);
        ASSERT_EQ(cps, (size_t)3);
        ASSERT_EQ(skipped, (size_t)PHC_FILLER);
        ASSERT(phc_decide(rx, &k.both, &revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT(phc_decide(rx, &k.two, &revoked, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        vcs_proof_receiver_free(rx);
        vcs_package_store_close(store);
#if !defined(_WIN32)
        /* A process dies after scanning every page, before publishing. The
         * next process rebuilds the same truth from the untouched CAS. */
        ASSERT(phc_crash_rebuild(dir, rows));
        store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        rx = phc_rebuilt(store, rows);
        ASSERT(rx != NULL);
        failures += phc_expect_unrevoked(rx, &k);
        vcs_proof_receiver_free(rx);
        vcs_package_store_close(store);
#endif
        test_rm_rf(dir);
    } TEST_END
    return failures;
}

/* ── a fork visible only past the first catalog page ────────────── */

static int phc_case_late_page_fork(void)
{
    int failures = 0;
    TEST_CASE("proof_history: a fork found only on a later page never hits") {
        ASSERT(phc_fresh());
        struct vcs_component_proof_key_v1 forked_key;
        phc_key("unit/forked", &forked_key);
        uint8_t a[1][VCS_PROOF_TICKET_WIRE_BYTES];
        uint8_t b[1][VCS_PROOF_TICKET_WIRE_BYTES];
        ASSERT(ptf_emit(&g_h, PTF_A, &g_h.base, ptf_pass(), a[0], NULL));
        ASSERT(ptf_emit(&g_h, PTF_B, &g_h.base, ptf_pass(), b[0], NULL));

        /* A's own key signs a second history at the same sequence. */
        struct vcs_proof_issuer_log *fork =
            vcs_proof_issuer_log_new(g_h.seed[PTF_A]);
        ASSERT(fork != NULL);
        uint8_t forked[VCS_PROOF_TICKET_WIRE_BYTES];
        uint8_t fork_cp[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
        uint8_t scratch[VCS_PROOF_TICKET_WIRE_BYTES];
        struct vcs_proof_ticket_v1 t;
        bool made = ptf_emit(&g_h, PTF_STRANGER, &forked_key, ptf_pass(),
                             scratch, NULL) &&
                    vcs_proof_ticket_decode(scratch, sizeof(scratch), &t) &&
                    vcs_proof_issuer_log_append(fork, &t, forked) &&
                    vcs_proof_issuer_log_checkpoint(fork, 51, fork_cp);
        vcs_proof_issuer_log_free(fork);
        ASSERT(made);
        uint8_t forked_root[32], fork_cp_root[32];
        ASSERT(vcs_blob_root(forked, sizeof(forked), forked_root));
        ASSERT(vcs_blob_root(fork_cp, sizeof(fork_cp), fork_cp_root));
        const uint8_t *late = memcmp(forked_root, fork_cp_root, 32) > 0
                            ? forked_root : fork_cp_root;

        char dir[256];
        test_make_tmpdir(dir, sizeof(dir), "proof_history", "latefork");
        struct vcs_package_store *store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        ASSERT(phc_fill(store, late, VCS_PACKAGE_STORE_PAGE_MAX));
        uint8_t cp_a[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
        uint8_t cp_b[VCS_PROOF_CHECKPOINT_WIRE_BYTES];
        ASSERT(phc_store_log(store, PTF_A, a, 1, 52, cp_a));
        ASSERT(phc_store_log(store, PTF_B, b, 1, 53, cp_b));
        /* A live receiver already serves a HIT from the honest history. */
        ASSERT(phc_sync_one(cp_a, a[0]));
        ASSERT(phc_sync_one(cp_b, b[0]));
        struct vcs_proof_ticket_class cls[PHC_CAP];
        struct vcs_proof_reuse_decision d;
        ASSERT(phc_decide(g_h.rx, &g_h.base, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_HIT_PASS);
        ASSERT(phc_put(store, forked, sizeof(forked), NULL));
        ASSERT(phc_put(store, fork_cp, sizeof(fork_cp), NULL));
        const size_t rows = VCS_PACKAGE_STORE_PAGE_MAX + 6u;
        struct vcs_package_store_summary page_rows[VCS_PACKAGE_STORE_PAGE_MAX];
        struct vcs_package_store_page page;
        ASSERT_EQ(vcs_package_store_page_summaries(
                      store, NULL, VCS_PACKAGE_STORE_PAGE_MAX, 0, page_rows,
                      &page), VCS_PACKAGE_STORE_PAGE_OK);
        ASSERT(page.has_more);
        ASSERT(memcmp(late, page.next_root, 32) > 0);

        /* One page is not the history: refuse, publish nothing. */
        struct vcs_proof_receiver *partial = vcs_proof_receiver_new();
        ASSERT(partial != NULL);
        size_t tickets = 9, cps = 9, skipped = 9;
        ASSERT(!vcs_proof_receiver_rebuild_bounded(
            partial, store, VCS_PACKAGE_STORE_PAGE_MAX, &tickets, &cps,
            &skipped));
        ASSERT_EQ(tickets, (size_t)0);
        ASSERT_EQ(vcs_proof_receiver_ticket_count(partial), (size_t)0);
        vcs_proof_receiver_free(partial);

        /* Restart: the complete catalog shows A equivocating. */
        vcs_package_store_close(store);
        store = vcs_package_store_open(dir, PHC_QUOTA);
        ASSERT(store != NULL);
        struct vcs_proof_receiver *rx = phc_rebuilt(store, rows);
        ASSERT(rx != NULL);
        ASSERT(vcs_proof_receiver_issuer_equivocating(rx, g_h.pub[PTF_A]));
        ASSERT(phc_decide(rx, &g_h.base, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_QUORUM);
        ASSERT_STR_EQ(phc_reason_of(cls, d.tickets_seen, PTF_A),
                      VCS_PROOF_TICKET_EQUIVOCATION);
        ASSERT(phc_decide(rx, &forked_key, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_INELIGIBLE);
        ASSERT_STR_EQ(phc_reason_of(cls, d.tickets_seen, PTF_A),
                      VCS_PROOF_TICKET_EQUIVOCATION);
        vcs_proof_receiver_free(rx);

        /* The receiver that already served a HIT cannot publish a view that
         * unverifies its own checkpoint, so its rebuild refuses. The signed
         * fork from the later page still reaches it: A stops hitting, B's
         * history and the rest of the old view are untouched. */
        ASSERT(!vcs_proof_receiver_rebuild_bounded(g_h.rx, store, rows,
                                                   &tickets, &cps, &skipped));
        ASSERT_EQ(tickets, (size_t)0);
        ASSERT_EQ(vcs_proof_receiver_ticket_count(g_h.rx), (size_t)2);
        ASSERT(vcs_proof_receiver_issuer_equivocating(g_h.rx, g_h.pub[PTF_A]));
        ASSERT(!vcs_proof_receiver_issuer_equivocating(g_h.rx, g_h.pub[PTF_B]));
        ASSERT(phc_decide(g_h.rx, &g_h.base, NULL, cls, &d));
        ASSERT_EQ(d.outcome, VCS_PROOF_REUSE_MISS);
        ASSERT_STR_EQ(d.reason, VCS_PROOF_REUSE_WHY_QUORUM);
        ASSERT_STR_EQ(phc_reason_of(cls, d.tickets_seen, PTF_A),
                      VCS_PROOF_TICKET_EQUIVOCATION);
        ASSERT_STR_EQ(phc_reason_of(cls, d.tickets_seen, PTF_B),
                      VCS_PROOF_TICKET_ELIGIBLE);
        /* A budget that cannot reach the later page records nothing. */
        struct vcs_proof_receiver *early = vcs_proof_receiver_new();
        ASSERT(early != NULL);
        ASSERT(phc_sync_one_into(early, cp_a, a[0]));
        ASSERT(!vcs_proof_receiver_rebuild_bounded(
            early, store, VCS_PACKAGE_STORE_PAGE_MAX, &tickets, &cps,
            &skipped));
        ASSERT(!vcs_proof_receiver_issuer_equivocating(early, g_h.pub[PTF_A]));
        vcs_proof_receiver_free(early);
        vcs_package_store_close(store);
        test_rm_rf(dir);
    } TEST_END
    return failures;
}

int ptf_history_cases(void)
{
    int failures = 0;
    failures += phc_case_revoked_after_reopen();
    failures += phc_case_late_page_fork();
    ptf_free(&g_h);
    return failures;
}
