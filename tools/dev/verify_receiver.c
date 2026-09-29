/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * The fixed result.c receiver inside a landing proof: admit a signed
 * separate-account observation before the bundle make, hand zcc only the
 * admitted bytes, and after make decide HIT, COLD or BLOCK from what was
 * actually published. See verify_receiver.h. */
#define _XOPEN_SOURCE 700
#include "verify_receiver.h"

#include "dev_proof_budget.h"
#include "dev_proof_signer.h"
#include "verify_receiver_internal.h"
#include "verify_store.h"
#include "verify/fixed_result_contract.h"

#include "base/hex.h"
#include "base/safe_alloc.h"
#include "platform/fd_path.h"
#include "platform/time_compat.h"
#include "sha3/sha3.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VR_TARGET_LEN ZCL_FR_TARGET_LEN
#define VR_SERVED_MAX 8u

static const char *const k_vr_work_names[] = {
    ZCL_VERIFY_RECEIVER_ARGV, ZCL_VERIFY_RECEIVER_OBJECT,
    ZCL_VERIFY_RECEIVER_DEP_TAIL, ZCL_VERIFY_RECEIVER_STDERR,
    ZCL_VERIFY_RECEIVER_LOG, "pp.i", "pp.d", "pp.stderr",
};

/* Everything one preparation measures; heap-allocated (the profile and the
 * expected key are large). */
struct vr_ctx {
    struct zcl_verify_receiver *r;
    int work_fd;
    int gen_fd;
    struct vr_bytes profile_raw;
    struct vr_profile profile;
    struct zcl_fixed_result_v2_roots pins;
    uint8_t pp_sha3[32];
    struct vr_bytes dep;
    struct zcl_fixed_result_v2_expected expected;
    const char *compiler;
    const char *generation;
    const struct zcl_verify_attest_box_key *box;
#ifdef ZCL_TESTING
    const struct zcl_verify_receiver_fixture *fixture;
#endif
};

static void vr_init(struct zcl_verify_receiver *r)
{
    memset(r, 0, sizeof(*r));
    r->verdict = ZCL_VERIFY_RECEIVER_COLD;
    r->reason = "receiver_unprepared";
    r->lock_fd = -1;
}

static void vr_set(struct zcl_verify_receiver *r,
                   enum zcl_verify_receiver_verdict verdict,
                   const char *reason)
{
    r->verdict = verdict;
    r->reason = reason;
}

static void vr_unlock(struct zcl_verify_receiver *r)
{
    if (r->lock_fd >= 0) (void)close(r->lock_fd);
    r->lock_fd = -1;
}

static int64_t vr_cpu_us(int who)
{
    struct rusage ru;
    if (getrusage(who, &ru) != 0) return 0;
    return (int64_t)(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000000 +
           (int64_t)(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec);
}

/* ── The driver-private work directory ─────────────────────────────────── */

/* Remove only the names this receiver writes; anything else keeps the
 * directory and refuses, so a planted entry is never silently adopted. */
static bool vr_work_clear(const char *work)
{
    int fd = open(work, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return errno == ENOENT;
    struct stat st;
    bool ok = fstat(fd, &st) == 0 && st.st_uid == geteuid();
    for (size_t i = 0; ok && i < sizeof(k_vr_work_names) /
                                     sizeof(k_vr_work_names[0]); i++)
        if (unlinkat(fd, k_vr_work_names[i], 0) != 0 && errno != ENOENT)
            ok = false;
    (void)close(fd);
    return ok && rmdir(work) == 0;
}

static const char *vr_work_fresh(struct vr_ctx *c, const char *work)
{
    struct zcl_verify_receiver *r = c->r;
    char parent[PATH_MAX];
    if (!work || work[0] != '/' ||
        snprintf(r->work, sizeof(r->work), "%s", work) >= (int)sizeof(r->work))
        return "receiver_work_unsafe";
    size_t g = strlen(r->generation);
    if (strncmp(work, r->generation, g) == 0 &&
        (work[g] == '/' || work[g] == 0))
        return "receiver_work_inside_generation";
    if (!vr_work_clear(work) || mkdir(work, 0700) != 0)
        return "receiver_work_unsafe";
    c->work_fd = open(work, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (c->work_fd < 0 || !realpath(work, parent) ||
        (strncmp(parent, r->generation, g) == 0 &&
         (parent[g] == '/' || parent[g] == 0)))
        return "receiver_work_unsafe";
    return NULL;
}

/* ── Measure, expect, look up, admit ───────────────────────────────────── */

static const char *vr_measure(struct vr_ctx *c)
{
    struct zcl_verify_receiver *r = c->r;
    static const char placeholder[] = ZCL_VERIFY_RECEIVER_PLACEHOLDER_TARGET;
    const char *why = vr_preprocess(c->compiler, r->generation, &c->profile,
                                    c->work_fd, r->work, c->pp_sha3, &c->dep);
    if (why) return why;
    why = zcl_verify_receiver_depfile_inputs(c->dep.p, c->dep.n, placeholder,
                                             &r->inputs, &r->input_count);
    if (why) return why;
    r->dep_tail_len = c->dep.n - VR_TARGET_LEN;
    r->dep_tail = zcl_malloc(r->dep_tail_len, "verify receiver dep tail");
    if (!r->dep_tail) return "receiver_out_of_memory";
    memcpy(r->dep_tail, c->dep.p + VR_TARGET_LEN, r->dep_tail_len);
    why = zcl_verify_receiver_source_content(c->gen_fd, r->inputs,
                                             r->input_count,
                                             r->source_content);
    if (why) return why;
    return memcmp(r->source_content, c->pins.source_content, 32u) == 0
               ? NULL : "receiver_source_content_mismatch";
}

/* The -c argv the proof's zcc is expected to run, with its private output
 * directory as /proc/self/fd/<n>: the key constructor checks every token
 * against the profile and the fixed environment. */
static const char *vr_expect(struct vr_ctx *c)
{
    static const char placeholder[] = ZCL_VERIFY_RECEIVER_PLACEHOLDER_TARGET;
    static const char *const env[] = {
        "LC_ALL=C", "TZ=UTC", "TMPDIR=/tmp", "PATH=/usr/bin:/bin"
    };
    char dep[PATH_MAX], obj[PATH_MAX];
    if (!platform_dirfd_child_path(dep, sizeof(dep), c->work_fd, "result.d") ||
        !platform_dirfd_child_path(obj, sizeof(obj), c->work_fd, "result.o"))
        return "receiver_work_unsafe";
    const char *argv[VR_PROFILE_TOKENS + 10u];
    size_t n = 0;
    for (size_t i = 0; i < VR_PROFILE_TOKENS; i++)
        argv[n++] = c->profile.tokens[i];
    const char *tail[] = {"-MMD", "-MP", "-MF", dep, "-MT", placeholder,
                          "-c", "-o", obj, ZCL_FR_SOURCE};
    for (size_t i = 0; i < 10u; i++) argv[n++] = tail[i];
    const char *why = NULL;
    if (!zcl_fixed_result_expected_v2(
            &c->pins, c->r->source_content, c->pp_sha3, placeholder,
            c->profile_raw.p, c->profile_raw.n, c->r->generation, argv, n,
            env, 4u, c->work_fd, &c->expected, &why))
        return why ? why : "fixed_result_v2_arguments_invalid";
    return NULL;
}

static bool vr_write_argv(const struct vr_ctx *c)
{
    size_t len = 0;
    for (size_t i = 0; i < VR_PROFILE_TOKENS; i++)
        len += strlen(c->profile.tokens[i]) + 1u;
    char *buf = zcl_malloc(len, "verify receiver argv");
    if (!buf) return false;
    size_t at = 0;
    for (size_t i = 0; i < VR_PROFILE_TOKENS; i++) {
        size_t n = strlen(c->profile.tokens[i]) + 1u;
        memcpy(buf + at, c->profile.tokens[i], n);
        at += n;
    }
    bool ok = vr_write_at(c->work_fd, ZCL_VERIFY_RECEIVER_ARGV, buf, len);
    free(buf);
    return ok;
}

/* The donor depfile names the donor's epoch target; everything after it
 * must equal this generation's own fresh depfile, byte for byte. */
static const char *vr_admit(struct vr_ctx *c, struct zcl_verify_store_result *s)
{
    struct zcl_verify_receiver *r = c->r;
    if (s->depfile_len <= VR_TARGET_LEN ||
        zcl_fr_target_check((const char *)s->depfile, VR_TARGET_LEN) ||
        s->depfile_len - VR_TARGET_LEN != r->dep_tail_len ||
        memcmp(s->depfile + VR_TARGET_LEN, r->dep_tail, r->dep_tail_len) != 0)
        return "receiver_depfile_mismatch";
    if (!vr_write_argv(c) ||
        !vr_write_at(c->work_fd, ZCL_VERIFY_RECEIVER_OBJECT, s->object,
                     s->object_len) ||
        !vr_write_at(c->work_fd, ZCL_VERIFY_RECEIVER_DEP_TAIL, r->dep_tail,
                     r->dep_tail_len) ||
        !vr_write_at(c->work_fd, ZCL_VERIFY_RECEIVER_STDERR, s->stderr_bytes,
                     s->stderr_len))
        return "receiver_work_unsafe";
    zcl_sha3_256(s->object, s->object_len, r->obj_sha3);
    r->obj_len = s->object_len;
    memcpy(r->store_key, s->store_key, sizeof(r->store_key));
    memcpy(r->record_sha3, s->record_sha3, sizeof(r->record_sha3));
    r->lock_fd = s->lock_fd;
    s->lock_fd = -1;
    return NULL;
}

static void vr_lookup(struct vr_ctx *c)
{
    struct zcl_verify_store_result s;
#ifdef ZCL_TESTING
    const struct zcl_verify_receiver_fixture *f = c->fixture;
    if (f)
        zcl_verify_store_lookup_site_fixture(f->site_anchor, f->allow_same_uid,
                                             &c->expected.expected, c->box, &s);
    else
#endif
        zcl_verify_store_lookup(&c->expected.expected, c->box, &s);
    if (s.verdict == ZCL_VERIFY_STORE_BLOCK)
        vr_set(c->r, ZCL_VERIFY_RECEIVER_BLOCK, s.reason);
    else if (s.verdict != ZCL_VERIFY_STORE_HIT)
        vr_set(c->r, ZCL_VERIFY_RECEIVER_COLD, s.reason);
    else {
        const char *why = vr_admit(c, &s);
        vr_set(c->r, why ? ZCL_VERIFY_RECEIVER_COLD : ZCL_VERIFY_RECEIVER_ADMITTED,
               why);
    }
    zcl_verify_store_result_release(&s);
}

/* After the trust root, pins and profile are in hand. */
static void vr_prepare_common(struct vr_ctx *c, const char *work)
{
    struct zcl_verify_receiver *r = c->r;
    const char *why = c->generation && realpath(c->generation, r->generation)
                          ? NULL : "receiver_generation_unreadable";
    if (!why) why = vr_profile_load(&c->profile_raw, r->generation, &c->profile);
    if (!why) why = vr_work_fresh(c, work);
    if (!why) {
        c->gen_fd = open(r->generation, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (c->gen_fd < 0) why = "receiver_generation_unreadable";
    }
    if (!why) why = vr_measure(c);
    if (!why) why = vr_expect(c);
    if (why) {
        vr_set(r, ZCL_VERIFY_RECEIVER_COLD, why);
        return;
    }
    vr_lookup(c);
}

static struct vr_ctx *vr_ctx_new(struct zcl_verify_receiver *r,
                                 const char *generation,
                                 const struct zcl_verify_attest_box_key *box)
{
    vr_init(r);
    r->wall_us = platform_time_monotonic_us();
    r->cpu_us = vr_cpu_us(RUSAGE_SELF) + vr_cpu_us(RUSAGE_CHILDREN);
    struct vr_ctx *c = zcl_calloc(1, sizeof(*c), "verify receiver context");
    if (!c) {
        vr_set(r, ZCL_VERIFY_RECEIVER_COLD, "receiver_out_of_memory");
        return NULL;
    }
    c->r = r;
    c->work_fd = c->gen_fd = -1;
    c->box = box;
    c->compiler = ZCL_VERIFY_RECEIVER_COMPILER;
    c->generation = generation;
    return c;
}

static void vr_ctx_done(struct vr_ctx *c, struct zcl_verify_receiver *r)
{
    if (c) {
        if (c->work_fd >= 0) (void)close(c->work_fd);
        if (c->gen_fd >= 0) (void)close(c->gen_fd);
        free(c->profile_raw.p);
        free(c->dep.p);
        free(c);
    }
    if (r->verdict != ZCL_VERIFY_RECEIVER_ADMITTED) vr_unlock(r);
    r->wall_us = platform_time_monotonic_us() - r->wall_us;
    r->cpu_us = vr_cpu_us(RUSAGE_SELF) + vr_cpu_us(RUSAGE_CHILDREN) -
                r->cpu_us;
}

/* The key first: with no installed verifier nothing else is read and no
 * compiler is launched. */
static const char *vr_trust_first(const struct zcl_verify_attest_box_key *box)
{
    struct zcl_verify_attest_trust_root root;
    const char *why = NULL;
    if (!zcl_verify_attest_trust_root_load(NULL, box, &root, &why))
        return why ? why : ZCL_VERIFY_ATTEST_WHY_NO_VERIFIER_KEY;
#if !defined(__linux__)
    return "receiver_platform_unsupported";
#else
    return NULL;
#endif
}

void zcl_verify_receiver_prepare(const char *generation, const char *work_dir,
                                 const struct zcl_verify_attest_box_key *box,
                                 struct zcl_verify_receiver *out)
{
    struct vr_ctx *c = vr_ctx_new(out, generation, box);
    const char *why = c ? vr_trust_first(box) : NULL;
    if (c && !why && !zcl_verify_store_pins_load(&c->pins, &why) && !why)
        why = "store_pins_missing";
    if (c && !why)
        why = vr_read_root_owned(ZCL_VERIFY_RECEIVER_PROFILE_PATH,
                                 VR_PROFILE_MAX, &c->profile_raw);
    if (why) vr_set(out, ZCL_VERIFY_RECEIVER_COLD, why);
    else if (c) vr_prepare_common(c, work_dir);
    vr_ctx_done(c, out);
}

#ifdef ZCL_TESTING
/* The pins from anchor/etc/z23verify under the loader's custody checks,
 * with the anchor's owner (the test uid) standing in for root. */
static const char *vr_fixture_pins(const struct zcl_verify_receiver_fixture *f,
                                   struct zcl_fixed_result_v2_roots *out)
{
    char dir[PATH_MAX];
    if (!f || !f->site_anchor ||
        snprintf(dir, sizeof(dir), "%s/etc/z23verify", f->site_anchor) >=
            (int)sizeof(dir))
        return "store_pins_path_unsafe";
    return zcl_verify_store_pins_load_fixture(dir, (unsigned)geteuid(),
                                              (unsigned)geteuid(), out);
}

void zcl_verify_receiver_prepare_fixture(
    const char *generation, const char *work_dir,
    const struct zcl_verify_attest_box_key *box,
    const struct zcl_verify_receiver_fixture *fixture,
    struct zcl_verify_receiver *out)
{
    struct vr_ctx *c = vr_ctx_new(out, generation, box);
    const char *why = c ? vr_trust_first(box) : NULL;
    if (c && !why) why = vr_fixture_pins(fixture, &c->pins);
    if (c && !why) {
        c->fixture = fixture;
        if (fixture->compiler) c->compiler = fixture->compiler;
        why = vr_read_at(AT_FDCWD, fixture->profile_path, VR_PROFILE_MAX,
                         &c->profile_raw);
    }
    if (why) vr_set(out, ZCL_VERIFY_RECEIVER_COLD, why);
    else if (c) vr_prepare_common(c, work_dir);
    vr_ctx_done(c, out);
}
#endif

/* ── The consuming step's environment ──────────────────────────────────── */

bool zcl_verify_receiver_env_apply(const struct zcl_verify_receiver *r)
{
    char log[PATH_MAX];
    if (!r || r->verdict != ZCL_VERIFY_RECEIVER_ADMITTED)
        return zcl_verify_receiver_env_clear();
    return snprintf(log, sizeof(log), "%s/%s", r->work,
                    ZCL_VERIFY_RECEIVER_LOG) < (int)sizeof(log) &&
           setenv(ZCL_VERIFY_RECEIVER_ENV, r->work, 1) == 0 &&
           setenv("ZCC_LOG", log, 1) == 0;
}

bool zcl_verify_receiver_env_clear(void)
{
    return unsetenv(ZCL_VERIFY_RECEIVER_ENV) == 0 && unsetenv("ZCC_LOG") == 0;
}

/* ── After the step ────────────────────────────────────────────────────── */

struct vr_log {
    char served[VR_SERVED_MAX][VR_TARGET_LEN + 1u];
    size_t served_count;
    bool served_overflow;
    char miss[64];
    unsigned launches;
};

static bool vr_token_ok(const char *s, size_t n)
{
    if (n == 0 || n >= 48) return false;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') ||
              s[i] == '_'))
            return false;
    return true;
}

/* One zcc log line: "<disposition> <detail> <output>", space padded. */
static void vr_log_line(struct vr_log *l, char *line)
{
    char *save = NULL;
    char *disposition = strtok_r(line, " ", &save);
    char *detail = disposition ? strtok_r(NULL, " ", &save) : NULL;
    char *path = detail ? strtok_r(NULL, " ", &save) : NULL;
    if (!path) return;
    static const char admitted[] = "admitted:";
    if (strcmp(disposition, "MISS") == 0 &&
        strncmp(detail, "verified:", 9) == 0)
        l->launches++;
    else if (strcmp(disposition, "MISS") == 0 &&
             strncmp(detail, admitted, sizeof(admitted) - 1u) == 0 &&
             !l->miss[0] &&
             vr_token_ok(detail + sizeof(admitted) - 1u,
                         strlen(detail + sizeof(admitted) - 1u)))
        (void)snprintf(l->miss, sizeof(l->miss), "%s",
                       detail + sizeof(admitted) - 1u);
    else if (strcmp(disposition, "VERIFIED") == 0 &&
             l->served_count < VR_SERVED_MAX &&
             strlen(path) == VR_TARGET_LEN)
        memcpy(l->served[l->served_count++], path, VR_TARGET_LEN + 1u);
    else if (strcmp(disposition, "VERIFIED") == 0)
        l->served_overflow = true;
}

static void vr_log_read(const struct zcl_verify_receiver *r, struct vr_log *l)
{
    memset(l, 0, sizeof(*l));
    int fd = open(r->work, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct vr_bytes b = {0};
    if (fd < 0 ||
        vr_read_at(fd, ZCL_VERIFY_RECEIVER_LOG, VR_LOG_MAX, &b) != NULL) {
        if (fd >= 0) (void)close(fd);
        return;
    }
    (void)close(fd);
    char *save = NULL;
    for (char *line = strtok_r((char *)b.p, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save))
        vr_log_line(l, line);
    free(b.p);
}

/* The published object must be the admitted bytes and its depfile the
 * receiver's own, naming this exact epoch target. */
static const char *vr_served_check(struct zcl_verify_receiver *r, int gen_fd,
                                   const char *output)
{
    if (zcl_fr_target_check(output, strlen(output)))
        return "admitted_target_invalid";
    struct vr_bytes obj = {0}, dep = {0};
    uint8_t hash[32];
    char dep_path[VR_TARGET_LEN + 1u];
    memcpy(dep_path, output, VR_TARGET_LEN + 1u);
    dep_path[VR_TARGET_LEN - 1u] = 'd';
    const char *why = vr_read_beneath(gen_fd, output, VR_OBJECT_MAX, &obj);
    if (!why) {
        zcl_sha3_256(obj.p, obj.n, hash);
        if (obj.n != r->obj_len || memcmp(hash, r->obj_sha3, 32u) != 0)
            why = "admitted_object_mismatch";
    }
    if (!why) why = vr_read_beneath(gen_fd, dep_path, VR_DEP_MAX, &dep);
    if (!why && (dep.n != VR_TARGET_LEN + r->dep_tail_len ||
                 memcmp(dep.p, output, VR_TARGET_LEN) != 0 ||
                 memcmp(dep.p + VR_TARGET_LEN, r->dep_tail,
                        r->dep_tail_len) != 0))
        why = "receiver_depfile_mismatch";
    if (!why) r->bytes += obj.n + dep.n;
    free(obj.p);
    free(dep.p);
    return why ? (strcmp(why, "receiver_file_missing") == 0
                      ? "admitted_object_missing" : why)
               : NULL;
}

static const char *vr_consumed(struct zcl_verify_receiver *r,
                               const struct vr_log *l)
{
    if (l->served_overflow) return "admitted_served_overflow";
    int gen_fd = open(r->generation, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (gen_fd < 0) return "receiver_generation_unreadable";
    const char *why = NULL;
    for (size_t i = 0; !why && i < l->served_count; i++)
        why = vr_served_check(r, gen_fd, l->served[i]);
    uint8_t now[32];
    if (!why && (zcl_verify_receiver_source_content(gen_fd, r->inputs,
                                                    r->input_count, now) ||
                 memcmp(now, r->source_content, 32u) != 0))
        why = "receiver_source_changed";
    (void)close(gen_fd);
    return why;
}

void zcl_verify_receiver_finish(struct zcl_verify_receiver *r)
{
    if (!r) return;
    int64_t wall0 = platform_time_monotonic_us();
    int64_t cpu0 = vr_cpu_us(RUSAGE_SELF);
    struct vr_log l;
    vr_log_read(r, &l);
    r->compile_launches = l.launches;
    if (r->verdict == ZCL_VERIFY_RECEIVER_ADMITTED) {
        if (l.served_count == 0 && !l.served_overflow) {
            (void)snprintf(r->reason_buf, sizeof(r->reason_buf), "%s%s",
                           l.miss[0] ? "admitted_" : "",
                           l.miss[0] ? l.miss : "admitted_not_consumed");
            vr_set(r, ZCL_VERIFY_RECEIVER_COLD, r->reason_buf);
        } else {
            const char *why = vr_consumed(r, &l);
            vr_set(r, why ? ZCL_VERIFY_RECEIVER_BLOCK : ZCL_VERIFY_RECEIVER_HIT,
                   why);
            if (!why) r->launches_avoided = (unsigned)l.served_count;
        }
        vr_unlock(r);
    }
    r->wall_us += platform_time_monotonic_us() - wall0;
    r->cpu_us += vr_cpu_us(RUSAGE_SELF) - cpu0;
}

/* ── Reporting and release ─────────────────────────────────────────────── */

bool zcl_verify_receiver_admit_text(const struct zcl_verify_receiver *r,
                                    char *out, size_t cap)
{
    int n;
    if (!r || !out) return false;
    if (r->verdict == ZCL_VERIFY_RECEIVER_HIT ||
        r->verdict == ZCL_VERIFY_RECEIVER_ADMITTED)
        n = snprintf(out, cap, "%s(%s,%s)",
                     r->verdict == ZCL_VERIFY_RECEIVER_HIT ? "hit" : "admitted",
                     r->store_key, r->record_sha3);
    else
        n = snprintf(out, cap, "%s(%s)",
                     r->verdict == ZCL_VERIFY_RECEIVER_BLOCK ? "block" : "cold",
                     r->reason ? r->reason : "receiver_unprepared");
    return n > 0 && (size_t)n < cap;
}

bool zcl_verify_receiver_phases_write(const struct zcl_verify_receiver *r,
                                      const char *phases_path)
{
    char admit[256], value[128], cost[128];
    if (!r || !phases_path || !phases_path[0] ||
        !zcl_verify_receiver_admit_text(r, admit, sizeof(admit)))
        return false;
    (void)snprintf(value, sizeof(value), "%u", r->launches_avoided);
    (void)snprintf(cost, sizeof(cost),
                   "wall_us=%lld cpu_us=%lld bytes=%llu compile_launches=%u",
                   (long long)r->wall_us, (long long)r->cpu_us,
                   (unsigned long long)r->bytes, r->compile_launches);
    return zcl_dev_proof_phase_note(phases_path, "object_reuse_admit", admit) &&
           zcl_dev_proof_phase_note(phases_path, "compile_launches_avoided",
                                    value) &&
           zcl_dev_proof_phase_note(phases_path, "object_reuse_cost", cost);
}

void zcl_verify_receiver_release(struct zcl_verify_receiver *r)
{
    if (!r) return;
    vr_unlock(r);
    free(r->dep_tail);
    r->dep_tail = NULL;
    zcl_verify_receiver_paths_free(r->inputs, r->input_count);
    r->inputs = NULL;
    r->input_count = 0;
    if (r->work[0]) (void)vr_work_clear(r->work);
    r->work[0] = 0;
}

void zcl_verify_receiver_box_key(struct zcl_verify_attest_box_key *out)
{
    const char *why = NULL;
    memset(out, 0, sizeof(*out));
    out->known = zcl_dev_proof_signer_public(out->pubkey, &out->present, &why);
}
