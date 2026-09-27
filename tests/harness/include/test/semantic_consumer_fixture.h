/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The declaration-identity consumer fixture: a five-TU C tree around one header, its edit table and the affected TUs each edit must yield. */
#ifndef ZCL_TEST_SEMANTIC_CONSUMER_FIXTURE_H
#define ZCL_TEST_SEMANTIC_CONSUMER_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCX_DIR "engine/modules/cxn"
#define SCX_HEADER SCX_DIR "/include/cx.h"
#define SCX_A SCX_DIR "/src/cx_a.c"   /* cx_big's layout, a static cx_twice */
#define SCX_B SCX_DIR "/src/cx_b.c"   /* cx_count, CX_MODE in #if, cx_twice */
#define SCX_C SCX_DIR "/src/cx_c.c"   /* cx_sum (CX_SCALE, a local cx_small), cx_hook */
#define SCX_D SCX_DIR "/src/cx_d.c"   /* calls cx_sum, takes cx_hook's address, sizes cx_big_t */
#define SCX_E SCX_DIR "/src/cx_e.c"   /* reads the header, names nothing in it */
/* `#include "cx.h"` searches the includer's directory first. */
#define SCX_SHADOW SCX_DIR "/src/cx.h"
/* A build input: no compile records reading it. */
#define SCX_MAKEFILE SCX_DIR "/cx.mk"
#define SCX_FIXTURES "tests/fixtures/semantic_consumer"
#define SCX_FILE_COUNT 6
#define SCX_TU_COUNT 5

enum scx_variant {
    SCX_BASE,
    SCX_LAYOUT,     /* cx_big gains a field: its users cx_a and (cx_big_t) cx_d */
    SCX_MACRO,      /* CX_CAP 64 -> 65 sizes cx_big: only its two users */
    SCX_COND,       /* CX_MODE, tested in cx_b.c's #if, changes value */
    SCX_NESTED,     /* CX_BASE, used only inside CX_SCALE's body, changes */
    SCX_TYPEDEF,    /* cx_count becomes long: only the TU naming it */
    SCX_TAIL,       /* a comment after every declaration: nothing moves */
    SCX_TOP,        /* a comment before every declaration: positions move */
    SCX_SIGNATURE,  /* cx_sum's parameter type, in the header and cx_c.c */
    SCX_STATIC,     /* cx_a.c's static cx_twice body; cx_b.c has its own */
    SCX_ADDRESS,    /* cx_hook's body, whose address cx_d.c takes */
    SCX_SHADOWED,   /* a byte-identical cx.h appears beside the TUs */
    SCX_DRIFT,      /* a flag every TU compiles with, plus a tail comment */
    SCX_LOCAL,      /* CX_PAD sizes cx_small, named only in cx_sum's body */
    SCX_BUILD,      /* a makefile no compile records reading changes */
    SCX_TOOL,       /* ...and every compile gains a flag: every TU drifts */
    SCX_BODY,       /* cx_sum's body, declared in the header, in cx_c.c alone */
    SCX_VARIANT_COUNT
};

struct scx_edit {
    const char *name;         /* fixture directory: <name>/<tu>.zsm */
    const char *file, *from, *to;   /* one exact replacement, or NULL */
    const char *file2, *from2, *to2; /* a second one, or NULL */
    const char *add_path;     /* a file this variant adds, or NULL */
    const char *extra_flag;   /* a flag put before k_scx_flags, or NULL */
    /* What the consumer must say with the base as "before": the changed
     * files it is asked about, and per TU (k_scx_tus order) whether it is
     * affected and its reason (NULL: not in the universe, it reads no
     * changed file). */
    const char *changed[2];
    bool affected[SCX_TU_COUNT];
    const char *reason[SCX_TU_COUNT];
    const char *obligations; /* "" narrowed, else the fallback reason */
    const char *incomplete;  /* NULL: the universe is complete, else why not */
    bool universal;          /* nothing bounds the change: every group */
    /* Functions the verdict's seeds must include: the compile may re-emit
     * each, so the walk has to start from it. */
    const char *seeds[3];
};

extern const struct scx_edit k_scx_edits[SCX_VARIANT_COUNT];
extern const char *const k_scx_paths[SCX_FILE_COUNT]; /* header first */
extern const char *const k_scx_tus[SCX_TU_COUNT];
extern const char *const k_scx_flags[];
extern const size_t k_scx_nflags;

/* The bytes of `path` in variant v (heap, NUL-terminated), NULL when the
 * variant has no such file. */
char *scx_text(enum scx_variant v, const char *path, size_t *len);
/* Write every file of variant v under root (the base header too when the
 * variant adds a shadow). */
bool scx_write_tree(const char *root, enum scx_variant v);
/* Write root/build/scx_deps/<tu>.d for every TU of variant v: the depfile
 * graph the consumer cross-checks its universe against. */
bool scx_write_depfiles(const char *root, enum scx_variant v);

/* mkdir(path, 0755), true when it exists afterwards. */
bool scx_mkdir(const char *path);

/* ── the check (semantic_consumer_check.c) ─────────────────────────────── */

#include <stdio.h>

#include "devloop_facts.h"

/* Before and after manifest bytes per TU (k_scx_tus order); NULL absent. */
struct scx_evidence {
    const uint8_t *before[SCX_TU_COUNT], *after[SCX_TU_COUNT];
    size_t before_len[SCX_TU_COUNT], after_len[SCX_TU_COUNT];
    bool no_depfiles; /* leave the depfile graph out */
};

struct scx_result {
    struct zcl_devloop_plan plan;
    struct zcl_devloop_facts_verdict verdict;
    struct zcl_devloop_facts_report report;
};

/* Write variant v, its depfiles and a facts directory from `ev` under root,
 * and run the consumer on the variant's changed files. */
bool scx_consume(const char *root, enum scx_variant v,
                 const struct scx_evidence *ev, struct scx_result *out);
const struct zcl_devloop_facts_tu_verdict *
scx_tu_of(const struct scx_result *r, const char *path);
/* Disagreements with the edit table (one line each on `why` when set);
 * *unsafe counts table-affected TUs the consumer left unaffected. */
size_t scx_compare(enum scx_variant v, const struct scx_result *r,
                   size_t *unsafe, FILE *why);
void scx_result_free(struct scx_result *r);

#endif /* ZCL_TEST_SEMANTIC_CONSUMER_FIXTURE_H */
