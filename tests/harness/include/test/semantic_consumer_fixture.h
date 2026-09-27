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
/* A header only a __has_include names. */
#define SCX_OPT SCX_DIR "/include/cx_opt.h"
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
    /* Regressions from differential fuzzing, each against its own "before"
     * (a P_ variant, produced but not judged). */
    SCX_P_COUNTER,  /* cx.h gains cx_tick; cx_b.c expands __COUNTER__ */
    SCX_COUNTER,    /* cx_tick gains a __COUNTER__: cx_b.c's count moves */
    SCX_P_HSTATIC,  /* cx.h defines static cx_state; cx_b.c sets and reads it */
    SCX_HSTATIC,    /* cx_b.c stores 6, not 5: the reader may fold it */
    SCX_P_ALIAS,    /* cx_c.c aliases cx_sum_alias to cx_sum */
    SCX_ALIAS,      /* cx_sum's body: a second entry runs it */
    SCX_P_HASINC,   /* cx.h tests __has_include("cx_opt.h"); cx_b.c reads it */
    SCX_HASINC,     /* include/cx_opt.h is created: the test flips */
    SCX_HASDEL,     /* ...and deleted again (planned against SCX_HASINC) */
    SCX_P_CLEANUP,  /* cx_e.c: cx_work's local runs cleanup(cx_rel) */
    SCX_CLEANUP,    /* cx_rel's body: cx_work inlines it, naming it nowhere */
    SCX_P_UNITY,    /* cx_e.c includes cx_c.c, cx_sum renamed cx_e_sum */
    SCX_UNITY,      /* cx_sum's body: cx_e.c compiles it as cx_e_sum */
    SCX_P_UNITY_MOVE, /* unity, and cx_c.c defines cx_get reading cx_tail below it */
    SCX_UNITY_MOVE, /* a comment line moves cx_tail's declaration in cx_e.c's -g1 */
    SCX_P_UNITY2,   /* ...and cx_c.c defines cx_sum2 (__LINE__), cx_e_sum2 there */
    SCX_UNITY2,     /* cx_sum's body, and a line that moves cx_sum2 */
    SCX_P_CTR_UNITY, /* unity, cx_e.c expands __COUNTER__, cx_d.c calls cx_e_sum */
    SCX_CTR_UNITY,  /* cx_sum's body: the includer is broadened before any seed */
    SCX_P_UNITY_AB, /* cx_e.c includes cx_c.c and cx_a.c, renaming both */
    SCX_UNITY_AB,   /* both bodies change: each included .c seeds */
    SCX_VARIANT_COUNT
};

struct scx_edit {
    const char *name;         /* fixture directory: <name>/<tu>.zsm */
    const char *file, *from, *to;   /* one exact replacement, or NULL */
    const char *file2, *from2, *to2; /* a second one, or NULL */
    const char *file3, *from3, *to3; /* a third one, or NULL */
    const char *add_path;     /* a file this variant adds, or NULL */
    const char *add_body;     /* its bytes; NULL: the base header (a shadow) */
    const char *extra_flag;   /* a flag put before k_scx_flags, or NULL */
    /* What the consumer must say against `before` (the base unless named):
     * the changed files it is asked about, and per TU (k_scx_tus order)
     * whether it is affected and its reason (NULL: not in the universe, it
     * reads no changed file). */
    const char *changed[2];
    bool affected[SCX_TU_COUNT];
    const char *reason[SCX_TU_COUNT];
    const char *obligations; /* "" narrowed, else the fallback reason */
    const char *incomplete;  /* NULL: the universe is complete, else why not */
    bool universal;          /* nothing bounds the change: every group */
    /* Functions the verdict's seeds must include: the compile may re-emit
     * each, so the walk has to start from it. */
    const char *seeds[3];
    /* The variant this one is planned against instead of the base, and
     * whether it is only a "before" for another (never judged). */
    enum scx_variant before;
    bool pre;
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
