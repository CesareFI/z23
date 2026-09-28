/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Compiler-API-free core of the semantic manifest producer (the libclang sensor): paths, lookups, records. */
#ifndef ZCL_TOOLS_SENSORS_CLANG_MANIFEST_CORE_H
#define ZCL_TOOLS_SENSORS_CLANG_MANIFEST_CORE_H

/* The libclang sensor reads the front end through the libclang C API (a
 * second parse) and hands every observation to this core, which alone spells
 * paths, probes include slots, replays __has_include, and encodes every
 * record. A future producer over another compiler API (a native Z23 front
 * end) links the same core, so producers can only disagree about WHAT the
 * front end saw, never about how it is written down. This file includes no
 * compiler header and is plain C23. */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "vcs/semantic_manifest.h"
#include "vcs/semantic_namespace.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* One file the front end read. `key` is the producer's own file handle. */
struct cm_file {
    const void *key;
    char *path;       /* canonical manifest path */
    char *real;       /* realpath, for probe comparison */
    char *opened;     /* the name the front end opened it by */
    uint8_t origin;   /* VCS_SEMANTIC_ORIGIN_V1_* */
    const char *contents;
    size_t size;
};

/* One search directory: as the front end printed it, and canonical. */
struct cm_dir {
    char *raw;
    char *path;
};

struct cm_dirs {
    struct cm_dir *items;
    size_t n;
    size_t cap;
};

struct cm_macro {
    char *name;
    const char *path;
    char *body;
    uint8_t function_like;
};

struct cm_expansion {
    const struct cm_file *file;
    unsigned offset;
    char *name;
    char *def_id; /* canonical macro identity (facts only) */
    bool in_fn;   /* inside a function definition's span */
};

struct cm_function {
    const struct cm_file *file;
    char *name;
    char *type;
    char *id;     /* canonical function identity */
    uint8_t linkage;
    uint8_t token_sha3[32];
    char **callees;
    size_t ncallees;
    size_t capcallees;
    unsigned begin_line, end_line, begin_off, end_off;
};

/* One unknown-effect occurrence, counted at emission. */
struct cm_occurrence {
    char *site;
    uint8_t kind;
    char *detail;
};

struct cm_core {
    char root[PATH_MAX];
    size_t root_len;
    char home[PATH_MAX];
    size_t home_len;
    struct cm_file *files;
    size_t nfiles, capfiles;
    size_t file_hint;
    struct cm_dirs quote, angled, ignored;
    char *resource_dir;
    struct cm_macro *macros;
    size_t nmacros, capmacros;
    struct cm_expansion *exps;
    size_t nexps, capexps;
    struct cm_function *fns;
    size_t nfns, capfns;
    struct vcs_semantic_builder_v1 *b;
    /* facts extension */
    bool facts;
    /* The rule the front end layer spells canonical types by (a fixed
     * token, e.g. "clang_getTypePrettyPrinted"). It enters the producer
     * digest, so manifests written under two type grammars never compare
     * as the same producer. NULL: unknown, and the digest stays zero. */
    const char *type_grammar;
#if defined(__APPLE__)
    /* Type grammar plus the admitted C language/standard mode. */
    char producer_grammar[96];
    /* Bound to dyld before parsing; facts use these bytes only if the
     * post-extraction producer check agrees before the manifest is written. */
    uint8_t producer_before[32];
    bool producer_before_valid;
#endif
    struct vcs_semantic_namespace_v1 *ns;
    bool ns_bound;  /* every repo file read is the snapshot's exact bytes */
    struct cm_occurrence *occ;
    size_t nocc, capocc;
    /* The absolute realpath of the compiler that builds the object
     * (cm_resolve_cc), or NULL when the caller named none. */
    const char *object_cc;
    bool failed;
    char why[512];
};

/* The identity record's inputs. `compiler` names the front end that parsed;
 * cm_emit_identity appends the object compiler to it. */
struct cm_identity {
    const char *compiler;
    const char *triple;
    const char *main_path;
    char **argv;
    size_t argc;
};

/* ---- lifecycle and helpers ------------------------------------------------ */

/* Enter root (chdir), resolve $HOME (refused when unset, relative or
 * unresolvable), create the builder. */
bool cm_core_init(struct cm_core *c, const char *root);
void cm_core_free(struct cm_core *c);
bool cm_fail(struct cm_core *c, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
bool cm_grow(void **items, size_t *cap, size_t n, size_t elem);
char *cm_strdup(const char *s);
char *cm_strndup(const char *s, size_t n);
bool cm_add(struct cm_core *c, enum vcs_semantic_section_v1 section,
            struct vcs_semantic_record_v1 *rec);
bool cm_read_file(const char *path, uint8_t **out, size_t *len);
/* SHA3-256 of a stream's remaining bytes; closes fp (false for NULL). */
bool cm_stream_sha3(FILE *fp, uint8_t out[32]);
void cm_hex(const uint8_t d[32], char out[65]);

/* ---- paths (clang_manifest_paths.c) -------------------------------------- */

bool cm_norm_path(struct cm_core *c, const char *path, char **out);
bool cm_norm_arg(struct cm_core *c, const char *arg, char **out);
/* Canonicalize an absolute realpath to the manifest spelling. */
bool cm_spell_real(struct cm_core *c, const char *abs, char **out);
/* realpath when it resolves, else lexical, of a name relative to root. */
bool cm_absolute(struct cm_core *c, const char *path, char out[PATH_MAX]);
bool cm_push_dir(struct cm_core *c, struct cm_dirs *d, const char *raw,
                 size_t len);
/* The resource dir: the angled system dir holding __stddef_max_align_t.h. */
void cm_find_resource_dir(struct cm_core *c);

/* Add one file; `real` is its absolute realpath, `opened` its opened name. */
bool cm_file_add(struct cm_core *c, const void *key, const char *real,
                 const char *opened, bool is_main, const char *contents,
                 size_t size);
const struct cm_file *cm_file_by_key(struct cm_core *c, const void *key);
bool cm_emit_files(struct cm_core *c);

/* Output-only argv controls: 1 drop this, 2 drop this and the next. */
int cm_output_arg(const char *a);
/* The absolute realpath of the compiler command `cc`: as given (relative to
 * the current directory) when it holds a '/', else the first executable
 * regular file of that name on PATH, as execvp() would run it. Call before
 * cm_core_init, which enters the root. False when nothing resolves. */
bool cm_resolve_cc(const char *cc, char out[PATH_MAX]);
/* IDENTITY's compiler text is "<front end>; object-cc <path> sha3-256 <hex>"
 * (the spelled realpath of c->object_cc and the SHA3-256 of its bytes), or
 * "<front end>; object-cc unknown" without one. */
bool cm_emit_identity(struct cm_core *c, const struct cm_identity *id);

/* ---- lookups (clang_manifest_lookup.c) ------------------------------------ */

/* One #include/#include_next directive the front end resolved to `hit`. */
bool cm_lookup_directive(struct cm_core *c, const struct cm_file *includer,
                         const char *spelled, uint8_t form, uint8_t kind,
                         bool computed, const struct cm_file *hit);
/* Every conditional lookup (__has_include and its relatives, #embed) in a
 * repo file: replayed when its operand is a literal, else recorded with no
 * negative claim. */
bool cm_scan_has_include(struct cm_core *c);

/* ---- records (clang_manifest_records.c) ----------------------------------- */

bool cm_emit_decl(struct cm_core *c, const struct cm_file *f, const char *kind,
                  const char *name, const char *type, uint8_t linkage);
struct cm_field {
    char *name;
    char *type;
    uint64_t offset_bits;
    uint32_t bit_width;
};
bool cm_emit_layout(struct cm_core *c, const struct cm_file *f,
                    const char *name, uint8_t kind, uint64_t size,
                    uint64_t align, const struct cm_field *fields,
                    size_t nfields);
bool cm_emit_enum(struct cm_core *c, const struct cm_file *f,
                  const char *enum_name, const char *constant, int64_t value);
/* Takes ownership of name and body; `offset` is the name token's offset. */
bool cm_macro_def(struct cm_core *c, const struct cm_file *f, unsigned offset,
                  char *name, char *body);
bool cm_macro_exp(struct cm_core *c, const struct cm_file *f, unsigned offset,
                  char *name, char *def_id);
/* Takes ownership of fn's strings; returns the stored function. */
struct cm_function *cm_function_add(struct cm_core *c,
                                    const struct cm_function *fn);
bool cm_function_callee(struct cm_core *c, struct cm_function *fn,
                        const char *name);
bool cm_emit_deferred(struct cm_core *c);
/* Facts revision 2 (clang_manifest_cond.c): one "@cond:<path>" MACRO ref
 * per identifier a conditional directive of <path> tests. */
bool cm_emit_conditionals(struct cm_core *c);

/* ---- facts (clang_manifest_facts.c) --------------------------------------- */

/* Canonical identity (vcs_semantic_id_v1); `external` for linkage 3 or 4. */
char *cm_id(char prefix, const char *path, const char *name, uint8_t linkage);
bool cm_symbol(struct cm_core *c, const char *id, const char *path,
               const char *kind, uint8_t linkage, bool defined);
bool cm_ref(struct cm_core *c, const char *from, uint8_t kind, const char *to);
bool cm_unknown(struct cm_core *c, const char *site, uint8_t kind,
                const char *detail);
/* Load the snapshot and check that it binds every repo file read. */
bool cm_facts_begin(struct cm_core *c, const char *tree_hex);
/* The producer digest this process writes into FACTS (clang_manifest_
 * producer.c), over its images and its type grammar; false, with out all
 * zero, when it cannot name itself or type_grammar is NULL. */
bool cm_producer_digest(const char *type_grammar, uint8_t out[32]);
bool cm_emit_facts(struct cm_core *c, uint32_t max_records,
                   uint64_t max_section_bytes);
/* The PROBES record of one lookup (claim: slots below hit_slot, minus
 * present, are asserted absent). */
bool cm_emit_probe(struct cm_core *c, const struct cm_file *includer,
                   const char *spelled, uint8_t form, uint8_t kind, bool claim,
                   uint32_t hit_slot, const uint32_t *present, size_t npresent);

/* Finish the builder into *bytes (caller frees); false, with c->why set,
 * when the builder refuses. */
bool cm_finish_bytes(struct cm_core *c, uint8_t **bytes, size_t *len);
/* Write bytes to path through a same-directory temporary and a rename. */
bool cm_write_file(const char *path, const uint8_t *b, size_t n);

/* ---- warm binding (clang_manifest_warm.c) -------------------------------- */

/* Does the tree under root still hold what manifest m describes, outside its
 * main file? Every other file it read (repo or system) still has its SHA3. A
 * lookup without a negative claim (include_next, a computed or an absolute
 * include) cannot be re-checked, so it fails too. The absent slots of every
 * lookup are checked after the reparse, by cm_warm_lookups_agree; includes
 * no lookup records, by cm_warm_shadows. On failure why names the first
 * input that moved. */
bool cm_warm_bound(const char *root, const uint8_t *m, size_t n, char *why,
                   size_t why_len);
/* Do two manifests agree on every non-main file both read? (A warm reparse
 * reuses a preamble that clang validated by size and time only; its FILES
 * must show the exact bytes the previous accepted manifest did.) */
bool cm_warm_files_agree(const uint8_t *prev, size_t prev_len,
                         const uint8_t *next, size_t next_len, char *why,
                         size_t why_len);
/* After a warm reparse: the warm manifest has the accepted one's IDENTITY
 * (the reparse derived the same search list) and every directive both
 * resolved has an identical LOOKUPS record. The warm extraction probes
 * every slot below each hit by stat, fresh, so a file that now sits in a
 * slot the accepted parse saw absent, a present slot that went away, or a
 * moved hit changes that record. */
bool cm_warm_lookups_agree(const uint8_t *prev, size_t prev_len,
                           const uint8_t *next, size_t next_len, char *why,
                           size_t why_len);
/* The FILES digest m records for its main file; false when there is none. */
bool cm_warm_main_digest(const uint8_t *m, size_t n, uint8_t out[32]);
/* The shadow candidates of m, one per line into *out (caller frees; an empty
 * list is ""): for every non-main file, under every search dir that holds
 * it, each earlier search dir (quote dirs, then angled) that now holds the
 * same relative name ("<dir>/<name>"), or that no longer exists ("gone
 * <dir>/"). This covers the includes LOOKUPS does not record, those made
 * inside system headers, whose resolution a reused preamble never repeats.
 * False when m or a search dir cannot be read. */
bool cm_warm_shadows(const char *root, const uint8_t *m, size_t n, char **out,
                     size_t *out_len);
/* Are two shadow lists the same? On a difference why names the first line
 * that appeared or vanished. */
bool cm_warm_shadows_same(const char *then, size_t then_len, const char *now,
                          size_t now_len, char *why, size_t why_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCL_TOOLS_SENSORS_CLANG_MANIFEST_CORE_H */
