/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: One semantic manifest as a graph of canonical identities: each id's records and digest, its reference and closure edges, the TU's roots and its four identities. */
#ifndef ZCL_TOOLS_DEV_DEVLOOP_FACTS_INDEX_H
#define ZCL_TOOLS_DEV_DEVLOOP_FACTS_INDEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A reader-side model of one TU's manifest (facts revision 1 or 2). Every
 * record that belongs to an entity is attached to its canonical id:
 *   MACROS    m:<path>:<name>, and the name group m:<name> (every
 *             definition of that name the TU saw);
 *   LAYOUTS   s:/u:<path>:<name>; ENUMS k:<path>:<constant> and
 *             e:<path>:<enum>; DECLS and FUNCTIONS f:/v:<name> (external)
 *             or f:/v:/t:/s:/u:/e:<path>:<name>; SYMBOLS its id;
 *   REFS and UNKNOWNS their site (the from id), including the pseudo-sites
 *             "@scope:<path>" and "@cond:<path>" of facts revision 2.
 * An entity's digest hashes its records outside the main file; the main
 * file's records form the implementation root instead. Edges: every REFS
 * record, a tag named in a record's canonical type text ("struct N",
 * "union N", "enum N"; an unnamed tag reaches every anonymous or
 * typedef-named tag), a typedef to the tag of its name and path, and a
 * macro to the name group of every identifier in its body. */

struct fxi;

enum fxi_dirty {
    FXI_DIRTY_DIGEST = 1,   /* its records differ between the two sides */
    FXI_DIRTY_CHUNK = 2,    /* a changed header text chunk names it */
    FXI_DIRTY_POSITION = 4, /* a declaration the debug info records moved */
};

struct fxi_roots {
    uint8_t source[32];         /* every file read and its bytes */
    uint8_t fact[32];           /* the manifest's exact bytes */
    uint8_t interface[32];      /* digests of every id the TU reaches */
    uint8_t implementation[32]; /* the main file's bytes and records */
};

/* Index a valid manifest; NULL with *why set when it is invalid, carries no
 * facts extension, or memory runs out. The index points into m, which must
 * outlive it. */
struct fxi *fxi_open(const uint8_t *m, size_t n, const char **why);
void fxi_free(struct fxi *x);

const char *fxi_main(const struct fxi *x);
bool fxi_complete(const struct fxi *x);
uint8_t fxi_revision(const struct fxi *x);
const uint8_t *fxi_producer(const struct fxi *x);
/* The FILES digest of `path` in this TU, NULL when it did not read it. */
const uint8_t *fxi_file_digest(const struct fxi *x, const char *path);
size_t fxi_file_count(const struct fxi *x);
const char *fxi_file_path(const struct fxi *x, size_t k);
bool fxi_file_repo(const struct fxi *x, size_t k);
/* SHA3 of the records of one section (IDENTITY, LOOKUPS, PROBES...). */
void fxi_section_digest(const struct fxi *x, int section, uint8_t out[32]);

size_t fxi_count(const struct fxi *x);
const char *fxi_id(const struct fxi *x, size_t e);
const char *fxi_bare(const struct fxi *x, size_t e);
bool fxi_find(const struct fxi *x, const char *id, size_t *e);
const uint8_t *fxi_digest(const struct fxi *x, size_t e);
bool fxi_main_owned(const struct fxi *x, size_t e);
/* A record of e names `path` as its file (the file that declares it). */
bool fxi_has_path(const struct fxi *x, size_t e, const char *path);
/* The id is external (f:<name> or v:<name>). */
bool fxi_external(const struct fxi *x, size_t e);
/* e is a root: a main-file entity, an @scope/@cond site, or a definition
 * in another file the compile always emits (an external variable or
 * function defined in a header). */
bool fxi_root(const struct fxi *x, size_t e);
bool fxi_is_site(const struct fxi *x, size_t e, const char *prefix);
/* e is a function the main file defines (a FUNCTIONS record in it). */
bool fxi_main_function(const struct fxi *x, size_t e);
/* The line span [*lo, *hi] of e's function definition in `path`. */
bool fxi_span(const struct fxi *x, size_t e, const char *path, uint32_t *lo,
              uint32_t *hi);
/* A REFS record of this manifest names `id` with `kind` (0: any kind). */
bool fxi_refs_to(const struct fxi *x, const char *id, uint8_t kind);
/* An UNKNOWNS record at site e other than an external call. */
bool fxi_unknown_effect(const struct fxi *x, size_t e, uint8_t *kind);

/* Records of e, and the file the k-th one names (false: it names none). */
size_t fxi_nrows(const struct fxi *x, size_t e);
bool fxi_row_path_at(const struct fxi *x, size_t e, size_t k, const char **p,
                     size_t *n);
/* Edges: every REFS record (kind 1..6) and closure edge (kind 0). */
size_t fxi_edge_count(const struct fxi *x);
void fxi_edge_at(const struct fxi *x, size_t k, size_t *from, size_t *to,
                 uint8_t *kind);

void fxi_roots(const struct fxi *x, struct fxi_roots *out);

/* Taint: flags[e] (fxi_dirty bits) marks the dirty entities; on return
 * via[e] is, for every entity that reaches a dirty one along edges, the
 * dirty entity it reaches (SIZE_MAX when it reaches none). via has
 * fxi_count() slots. */
bool fxi_taint(const struct fxi *x, const uint8_t *flags, size_t *via);

#endif /* ZCL_TOOLS_DEV_DEVLOOP_FACTS_INDEX_H */
