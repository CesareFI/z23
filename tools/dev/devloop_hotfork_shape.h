/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * purpose: HOT_FORK shape admission. A capsule candidate is dlopen'ed into a
 * child of the resident image: every symbol it references but no longer
 * defines binds RTLD_LAZY to the resident's own (stale) copy, and every
 * constructor it carries runs at load. The HOT_FORK lane therefore admits
 * only implementation-only edits. The candidate unity object's ELF shape
 * (defined global symbols, writable and thread-local state, init/fini
 * sections) must equal the shape of the resident's own build objects,
 * build/dev-obj/epochs/<epoch>/<tu>.o for every TU of the capsule. Anything
 * else — including every missing or contradicting fact — is refused by a
 * named HOT_FORK_SHAPE_* reason and the save falls back to the restart path.
 *
 * Linux/ELF only; every other platform refuses by name.
 */
#ifndef ZCL_TOOLS_DEV_DEVLOOP_HOTFORK_SHAPE_H
#define ZCL_TOOLS_DEV_DEVLOOP_HOTFORK_SHAPE_H

#include <stdbool.h>
#include <stddef.h>

/* Every refusal's why-text starts with this prefix, then the reason name. */
#define ZCL_HOTFORK_SHAPE_REASON "HOT_FORK_SHAPE_"

/* The facts one HOT_FORK build is judged against, captured before the
 * candidate compile. `generation` names the resident image, its epoch build
 * session and the capsule's resident build objects; it joins the artifact
 * cache key, so a cached artifact is only ever reused inside the resident
 * generation that admitted it. `unbound` holds the refusal for the first
 * missing or contradicting fact ("" when every fact is present). */
struct zcl_hotfork_shape {
    const char *root;
    const char *source_tu;
    const char *sibling_tus;
    const char *adapter_id;
    const char *cc;
    const char *compiler_id;
    const char *cflags;
    char epoch[65];
    char toolchain[65];
    char generation[65];
    char unbound[320];
};

/* Captures the resident generation for one capsule. Never fails: a missing
 * fact is recorded in `unbound` and refused by zcl_hotfork_shape_admit(). */
void zcl_hotfork_shape_begin(struct zcl_hotfork_shape *shape, const char *root,
                             const char *source_tu, const char *sibling_tus,
                             const char *adapter_id, const char *cc,
                             const char *compiler_id, const char *cflags);

/* `prior` is the caller's own verdict so far; false passes through untouched
 * (its why-text is kept). Otherwise true only when the candidate object's
 * shape equals the resident build objects' shape inside an unchanged
 * generation, toolchain and dependency closure. `depfile` is the candidate
 * compile's make depfile; the per-capsule binding record is kept beside it
 * (".d" becomes ".shape") and is written only by an admitted candidate. */
bool zcl_hotfork_shape_admit(bool prior, const struct zcl_hotfork_shape *shape,
                             const char *candidate_object, const char *depfile,
                             char *why, size_t why_len);

/* True when `why` is a HOT_FORK shape refusal (a restart fallback), not a
 * compile failure. */
bool zcl_hotfork_shape_refused(const char *why);

#endif /* ZCL_TOOLS_DEV_DEVLOOP_HOTFORK_SHAPE_H */
