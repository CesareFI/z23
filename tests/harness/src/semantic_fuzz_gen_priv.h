/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts fuzz generator's private model: the random multi-TU project, its text buffer and the renderers semantic_fuzz_gen.c and semantic_fuzz_templ.c share. */
#ifndef ZCL_TEST_SEMANTIC_FUZZ_GEN_PRIV_H
#define ZCL_TEST_SEMANTIC_FUZZ_GEN_PRIV_H

#include "base/format_attribute.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SFZ_MAXH 4
#define SFZ_MAXT 8
#define SFZ_NTEMPL 42

/* A growing text buffer; `bad` latches the first failed allocation. */
struct sfz_buf {
    char *p;
    size_t n, cap;
    bool bad;
};

void sfz_bp(struct sfz_buf *b, const char *fmt, ...) ZCL_PRINTF_LIKE(2, 3);

struct sfz_hdr {
    int dir;                /* 1 = inc1, 2 = inc2 */
    int inc_prev;           /* includes the previous header */
    int A, Bplus, Fmul, Fadd;
    int C_opt, C_else, D_thr;
    int opt_defined;        /* #define HK_OPT inside the header */
    int i0type;             /* index into k_sfz_types */
    int extra_field;        /* struct gains a trailing field */
    int swap_fields;        /* first two named fields swapped */
    int in_type;            /* type of the nested struct's s field */
    int has_union, has_anon;
    int E0, E2;
    int fn_long;            /* hK_fn returns long */
    int fn2_param;          /* index into k_sfz_types for hK_fn2's parameter */
    int inl_add, inl_ctr;   /* hK_inl constant and __COUNTER__ uses */
    int inl2_add;
    int sa_min;
    int ctr_add;            /* HK_CTR = (__COUNTER__ + ctr_add) */
    int owner;              /* TU defining hK_fn / hK_fn2 / hK_var */
    int var_init, cvar_init;
    int comment;            /* 0 none, 1 top comment, 2 comment in inline body,
                               3 blank line before inline fn, 4 macro body comment,
                               5 trailing whitespace on a macro */
    int weak_fn;            /* hK_fn defined weak in owner */
    int X_thr;              /* H{k}_X chooses by #if H{k-1}_A > X_thr */
    int tab1;               /* h{k}_tab[1] */
    int new_lim;            /* header defines T_LIM (0 none) */
    int ctr_macro_n;        /* __COUNTER__ uses in H{k}_CTR */
    int opt_file;           /* inc1/h{k}_opt.h exists (a __has_include probe) */
    int opt_inc;            /* ...and the header includes it when present */
    int K;                  /* constexpr h{k}_K */
    /* the data layer (rendered only for the data_* kinds) */
    int idx;                /* H{k}_IDX, an index into h{k}_garr */
    int dtab1;              /* h{k}_dtab[1], a header static const table */
};

struct sfz_tu {
    int inc[SFZ_MAXH];      /* 0 not included, 1 quoted, 2 angled */
    int on[SFZ_NTEMPL];
    int k[SFZ_NTEMPL];      /* header used by the template */
    int c[SFZ_NTEMPL];      /* its constant */
    int cross;              /* TU that tN_cross calls */
    int m_thr;              /* main-file #if threshold */
    int sysinc;             /* includes <stddef.h> */
    int comment;            /* 0 none, 1 comment in body, 2 blank lines at top,
                               3 comment at file scope, 4 whitespace reflow in a
                               head, 5 blank line inside a body */
    int comment_t;          /* template the comment targets */
    int ctr_j;              /* template whose body gains a __COUNTER__ (-1 none) */
    /* the data layer (rendered only for the data_* kinds) */
    int str_n, str_pad;     /* tN_str: "sN-<str_n>" then str_pad x characters */
    int dt[4];              /* the static const table tN_dt */
};

struct sfz_model {
    int nh, nt;
    struct sfz_hdr h[SFZ_MAXH];
    struct sfz_tu t[SFZ_MAXT];
    char flags[256];
    int shadow_k;           /* -1 none */
    int shadow_where;       /* 0 inc1, 1 src */
    int shadow_variant;     /* 0 identical, 1 A+1, 2 inline body change */
    int util_owner;
    bool noctr, noline;     /* the profile drops __COUNTER__ / __LINE__ */
    bool data;              /* render the data layer */
};

enum {
    T_HELPER, T_S1, T_E1, T_INL, T_MAC, T_GEN, T_LAY, T_ENUM, T_PTR, T_WEAK,
    T_CTOR, T_LINE, T_SZ, T_TYPEOF, T_CROSS, T_COND, T_HFN, T_S2, T_OPT, T_CTR,
    T_INL2, T_FN2, T_SV, T_DCOND, T_CVAR, T_ALIGN, T_F, T_D, T_HERE, T_GV,
    T_UTIL, T_XD, T_TAB, T_LIM, T_KC, T_ALIAS, T_UALIAS, T_HAS, T_GC, T_K, T_HST, T_CLEAN
};

extern const char *const k_sfz_types[6];

/* semantic_fuzz_templ.c: the text of header k and of TU i. */
void sfz_render_header(const struct sfz_model *m, int k, const struct sfz_hdr *h,
                       struct sfz_buf *b);
void sfz_render_tu(const struct sfz_model *m, int i, struct sfz_buf *b);

#endif /* ZCL_TEST_SEMANTIC_FUZZ_GEN_PRIV_H */
