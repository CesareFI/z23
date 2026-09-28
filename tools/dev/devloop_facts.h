/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Private seams of the facts-narrowed closure: its file-scope text digest and its after-manifest binding. */
#ifndef ZCL_TOOLS_DEV_DEVLOOP_FACTS_H
#define ZCL_TOOLS_DEV_DEVLOOP_FACTS_H

#include "devloop.h"

/* A byte range [begin, end) of the main file owned by function definitions. */
struct zcl_devloop_facts_range {
    size_t begin, end;
};

/* Digest of `src` outside `keep` (sorted, non-overlapping, within src_len):
 * each gap transcribed with comment and whitespace runs collapsed to one
 * byte. See devloop_facts_text.c for exactly what it binds. */
void zcl_devloop_facts_text_digest(const uint8_t *src, size_t src_len,
                                   const struct zcl_devloop_facts_range *keep,
                                   size_t nkeep, uint8_t out[32]);

/* Digest of one function definition's head in src[begin, end): the text
 * before its body's first '{' outside parentheses, brackets, comments and
 * literals (storage class, GNU __attribute__ and C23 [[attributes]], return
 * type, declarator), with every comment and whitespace run collapsed to one
 * space. A head with a '#' byte keeps newlines apart from spaces, as the
 * file-scope digest does. The whole definition is head when it has no such
 * brace. */
void zcl_devloop_facts_head_digest(const uint8_t *src, size_t begin,
                                   size_t end, uint8_t out[32]);

/* Rule 9 (devloop_facts_bind.c): the after manifest of `tu` describes the
 * tree under root. False, with *reason "after-stale" or "lookup-unbound"
 * and a detail, when a file it read changed or a slot it saw absent now
 * exists. */
bool zcl_devloop_facts_bind_after(const char *root,
                                  const struct zcl_devloop_facts_tu *tu,
                                  const char **reason, char *detail,
                                  size_t detail_len);

/* ── evidence files (devloop_facts_json.c) ─────────────────────────────── */

/* Read <root>/<dir>/<file><suffix> (dir may be NULL) whole, at most `max`
 * bytes, into a zcl_malloc buffer with a NUL after it. False when it is
 * absent, larger or unreadable. */
bool zcl_devloop_facts_read(const char *root, const char *dir, const char *file,
                            const char *suffix, size_t max, uint8_t **out,
                            size_t *len);

/* zcl_devloop_plan_add_closure_facts() with the facts directory the walk
 * filters callers by (NULL: every caller by name is kept). */
bool zcl_devloop_facts_add_closure_in(
    const char *repo_root, const char *const *files, size_t file_count,
    const struct zcl_devloop_facts_tu *tus, size_t tu_count,
    const char *facts_dir, struct zcl_devloop_plan *plan,
    struct zcl_devloop_facts_verdict *verdict);

/* ── the narrowed walk (devloop_facts_walk.c) ──────────────────────────── */

/* One seed of the caller walk: its bare name and canonical id ("" when not
 * known: every caller by name is then kept). */
struct zcl_devloop_facts_seed {
    char name[ZCL_DEVLOOP_FACTS_NAME_MAX];
    char id[ZCL_DEVLOOP_FACTS_ID_MAX];
};

/* Replace the plan's closure with `fold` (files reached by construction)
 * plus the callers of `seeds`, callers of callers, CI_CLOSURE_DEFAULT_DEPTH
 * deep, as the file-seeded closure walks. With a facts_dir, a caller file
 * whose after manifest is there is kept only when its REFS name the callee's
 * canonical id (a same-name static elsewhere is not a caller); a caller file
 * without one is kept by name. SEMANTIC is then INCOMPLETE
 * ("facts-narrowed"). False, with v->reason, when the walk is bounded or
 * errs; the plan is then unchanged. */
bool zcl_devloop_facts_narrow(const char *root, const char *facts_dir,
                              const char *const *fold, size_t nfold,
                              const struct zcl_devloop_facts_seed *seeds,
                              size_t nseeds, struct zcl_devloop_plan *plan,
                              struct zcl_devloop_facts_verdict *v);

/* ── the declaration-identity consumer (devloop_facts_consumer.c) ──────── */

#define ZCL_DEVLOOP_FACTS_TU_MAX 4096

/* One translation unit of the universe: the TUs whose manifests read a
 * changed file, and every TU the depfile graph says reads one. */
struct zcl_devloop_facts_tu_verdict {
    char path[ZCL_DEVLOOP_PATH_MAX];
    bool has_roots; /* the after manifest was read */
    uint8_t source[32], fact[32], interface[32], implementation[32];
    bool has_action, has_artifact;
    uint8_t action[32], artifact[32];
    const char *action_reason; /* why action is null ("" when present) */
    bool affected;
    bool broadened; /* its obligations are seeded by the whole file */
    /* affected for its object bytes only (a debug position moved): in the
     * compile set, with no test obligation of its own */
    bool compile_only;
    const char *reason; /* see docs/work/SEMANTIC_MANIFEST.md, "Consumer" */
    char detail[192];
};

/* The premises a guard reading may rest on (docs/work/SEMANTIC_MANIFEST.md,
 * "Guarded includes"): make runs for objects (MAKECMDGOALS holds no
 * vendor-ready, deploy or install goal), and the compile epoch a $(shell)
 * computes is one path component. */
#define ZCL_DEVLOOP_PREMISE_GOAL_BUILDS_OBJECTS 1u
#define ZCL_DEVLOOP_PREMISE_EPOCH_ONE_COMPONENT 2u
#define ZCL_DEVLOOP_GUARD_GLOBS 48
#define ZCL_DEVLOOP_GUARD_TEXT 192

/* A missing optional include the root makefile provably skips: the
 * directive read not taken, the premises that reading used and every path
 * or pattern it globbed with what that found, so a reviewer can falsify it. */
struct zcl_devloop_facts_guard {
    char include[ZCL_DEVLOOP_PATH_MAX];
    char include_at[ZCL_DEVLOOP_GUARD_TEXT]; /* file:line */
    char guard[ZCL_DEVLOOP_GUARD_TEXT];      /* the directive, as written */
    char guard_at[ZCL_DEVLOOP_GUARD_TEXT];
    unsigned premises; /* ZCL_DEVLOOP_PREMISE_* */
    size_t nglobs;
    char glob[ZCL_DEVLOOP_GUARD_GLOBS][ZCL_DEVLOOP_GUARD_TEXT];
    char found[ZCL_DEVLOOP_GUARD_GLOBS][ZCL_DEVLOOP_GUARD_TEXT]; /* space-separated */
};

struct zcl_devloop_facts_report {
    bool applied;       /* a universe was computed */
    bool complete;      /* every TU that reads a changed file is accounted */
    const char *reason; /* "" when complete, else why not */
    char detail[192];
    size_t ntus, naffected;
    struct zcl_devloop_facts_tu_verdict *tus; /* ntus, sorted by path */
    /* obligations */
    size_t plain_groups;            /* path + closure groups, file-seeded */
    bool plain_universal;           /* ...which reached the whole catalog */
    const char *obligations_reason; /* "" when narrowed, else the fallback */
    const char *group_reason[ZCL_DEVLOOP_MAX_PLAN_GROUPS];
    const char *path_reason;        /* reason of every path group */
    struct zcl_devloop_facts_guard *guards; /* includes read skipped */
    size_t nguards;
};

/* Plan `files` (plan already holds zcl_devloop_plan_files) with the
 * evidence under facts_dir. When every changed file is a .c the caller
 * passes their loaded pairs as `tus` and the per-function rule chain
 * decides (zcl_devloop_facts_add_closure_in); otherwise (tus NULL) the
 * consumer selects the affected TUs by declaration identity and seeds the
 * walk from the functions that reach a changed id. Either way the report
 * lists the universe with its identities. Fills plan, verdict and report
 * (free it with zcl_devloop_facts_report_free). A non-NULL verdict and
 * report are zeroed before anything can fail, so the free is safe on every
 * path. False only for invalid arguments or memory. */
bool zcl_devloop_facts_consume(const char *root, const char *const *files,
                               size_t n, const char *facts_dir,
                               const struct zcl_devloop_facts_tu *tus,
                               struct zcl_devloop_plan *plan,
                               struct zcl_devloop_facts_verdict *verdict,
                               struct zcl_devloop_facts_report *report);
void zcl_devloop_facts_report_free(struct zcl_devloop_facts_report *report);

#endif /* ZCL_TOOLS_DEV_DEVLOOP_FACTS_H */
