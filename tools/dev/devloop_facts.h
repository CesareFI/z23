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

#endif /* ZCL_TOOLS_DEV_DEVLOOP_FACTS_H */
