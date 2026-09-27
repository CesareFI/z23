/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts differential fuzzer: its project generator, fixed reproducers, relocatable-ELF function reader and the per-case oracle the semantic_facts_fuzz group runs. */
#ifndef ZCL_TEST_SEMANTIC_FUZZ_H
#define ZCL_TEST_SEMANTIC_FUZZ_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── the generator (semantic_fuzz_gen.c, semantic_fuzz_templ.c) ─────────── */

/* Profile bits: drop a mutation family so the others are not drowned out.
 * __COUNTER__ and __LINE__ users make most plans fall back
 * ("position-dependent"), so the no-ctr-line profile narrows far more. */
enum {
    SFZ_NO_CTR = 1u,  /* no __COUNTER__ anywhere, no counter_c mutation */
    SFZ_NO_LINE = 2u, /* no __LINE__ anywhere */
    SFZ_NO_FLAG = 4u, /* no flag mutation */
};

struct sfz_meta {
    char kind[32];    /* the mutation kind */
    char detail[256]; /* what it changed */
    int ntus;         /* src/t0.c .. src/t<ntus-1>.c */
};

/* Write <dir>/before/ and <dir>/after/: a random multi-TU C23 project
 * (src/, inc1/, inc2/ and a Makefile whose CFLAGS_EXTRA line carries the
 * project's extra flags) and the same project after one mutation, plus
 * <dir>/meta.txt. `kind` NULL draws one; a kind name forces it. The same
 * seed and profile always write the same bytes. */
bool sfz_generate(uint64_t seed, unsigned profile, const char *kind,
                  const char *dir, struct sfz_meta *meta);

/* True when `kind` names a mutation kind sfz_generate() can force. */
bool sfz_kind_known(const char *kind);

/* mkdir -p with mode 0700. */
bool sfz_mkdirs(const char *path);
/* Write `n` bytes to <dir>/<rel>, creating its directories. */
bool sfz_put(const char *dir, const char *rel, const char *text, size_t n);

/* ── the fixed reproducers (semantic_fuzz_repro.c) ──────────────────────── */

/* One file of a reproducer; NULL text: absent on that side. */
struct sfz_file {
    const char *path;
    const char *before, *after;
};

struct sfz_repro {
    const char *name;   /* F<n>_... a false negative found and fixed, or
                           pass_... a shape that must keep passing */
    const char *kind, *detail;
    bool gcc_deps;      /* the depfiles come from gcc, as the dev compile's */
    const char *known_red; /* non-NULL: expected to fail until this lands */
    const struct sfz_file *files;
    size_t nfiles;
};

extern const struct sfz_repro k_sfz_repros[];
extern const size_t k_sfz_nrepros;

/* Write <dir>/before/ and <dir>/after/ of reproducer r. */
bool sfz_write_repro(const struct sfz_repro *r, const char *dir);

/* ── relocatable ELF64 functions (semantic_fuzz_elf.c) ──────────────────── */

#define SFZ_NAME_MAX 128

/* One STT_FUNC symbol of an ET_REL object: its bytes and the RELA entries
 * that patch them, digested twice, with and without the addends. */
struct sfz_func {
    char name[SFZ_NAME_MAX];
    uint32_t shndx;
    uint64_t value, size;
    uint8_t full[32];    /* bytes, relocation offset, type, target, addend */
    uint8_t noadd[32];   /* the same without the addends */
};

struct sfz_funcs {
    struct sfz_func *v;
    size_t n;
};

/* Read every function of the x86-64 little-endian ET_REL image `img`.
 * Fails closed, naming the defect in err, on any other shape, any
 * out-of-range offset or an unterminated name. */
bool sfz_elf_funcs(const uint8_t *img, size_t n, struct sfz_funcs *out,
                   char *err, size_t errlen);
void sfz_funcs_free(struct sfz_funcs *f);
/* The k-th (0-based) function named `name`, or NULL. */
const struct sfz_func *sfz_func_find(const struct sfz_funcs *f,
                                     const char *name, size_t k);

/* The DT_RUNPATH (else DT_RPATH) string of an ELF64 executable or shared
 * object, from its section headers. False when there is none or the file
 * is not ELF64 little-endian. */
bool sfz_elf_runpath(const char *path, char *out, size_t outlen);

/* ── one differential case (semantic_fuzz_case.c) ───────────────────────── */

struct sfz_env {
    char sensor[PATH_MAX]; /* absolute build/bin/z23-clang-manifest */
    char clang[PATH_MAX];  /* the clang of the LLVM the sensor links */
    char gcc[PATH_MAX];    /* gcc for gcc_deps cases, "" when absent */
    int jobs;              /* compiles and sensor runs at once */
};

struct sfz_case {
    char label[96];        /* "seed=<n> profile=<p>" or the reproducer name */
    char dir[PATH_MAX];    /* holds before/ and after/ */
    char kind[32], detail[256];
    bool gcc_deps;
};

enum sfz_status { SFZ_PASS, SFZ_FAIL, SFZ_NOOP, SFZ_ERROR };

struct sfz_outcome {
    enum sfz_status status;
    bool narrowed;         /* the plan's verdict narrowed */
    char vreason[64];      /* its fallback reason, "" when narrowed */
    size_t tus, predicted, changed, missed, over;
    size_t cfun;           /* functions with new bytes in changed objects */
    size_t covered_seed, covered_other, reloc, notcov;
    size_t seeds;          /* seeds the verdict names */
    double plan_s;
    char why[2048];        /* the first false-negative lines, or the error */
};

/* Compile every TU before and after (clang -std=c23 -O1 with the
 * project's flags), sense each, plan the change set in process with the
 * facts, and judge: (1) every TU the plan leaves unaffected has a
 * byte-identical object; (2) every function with new bytes, beyond a
 * relocation addend, is a seed or is covered by a fallback, a missed TU or
 * a broadened TU after a header change, and so is every alias of a seed.
 * A PASS removes the case directory. */
bool sfz_run_case(const struct sfz_env *env, const struct sfz_case *c,
                  struct sfz_outcome *out);

#endif /* ZCL_TEST_SEMANTIC_FUZZ_H */
