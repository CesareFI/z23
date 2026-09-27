/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: libclang-side state of the optional semantic sensor (never linked into z23, z23-dev or core). */
#ifndef ZCL_TOOLS_SENSORS_CLANG_MANIFEST_H
#define ZCL_TOOLS_SENSORS_CLANG_MANIFEST_H

#include <clang-c/Index.h>

#include "clang_manifest_core.h"

/* Set by the Makefile's link probe: 1 when the libclang image exports
 * clang_getTypePrettyPrinted, 0 when it does not (Apple's libclang.dylib).
 * The two spell canonical types by different grammars; CM_TYPE_GRAMMAR is
 * the token the producer digest binds (clang_manifest_ast.c, cm_type_str). */
#ifndef CM_TYPE_PRETTY_PRINTED
#define CM_TYPE_PRETTY_PRINTED 0
#endif
#if CM_TYPE_PRETTY_PRINTED
#define CM_TYPE_GRAMMAR "clang_getTypePrettyPrinted:anon-tag-loc-off"
#else
#define CM_TYPE_GRAMMAR "clang_getTypeSpelling:anon-tag-loc-cut"
#endif

struct cm_tagname {
    CXCursor decl;
    char *name;
};

/* The walk over one definition (a function body or a variable
 * initializer): its identity, and the call sites already recorded so the
 * callee's DeclRefExpr is not also an address escape. */
struct cm_site {
    const char *id;
    struct cm_function *fn;
    CXSourceLocation *calls;
    size_t ncalls, capcalls;
};

struct cm_state {
    struct cm_core core;
    CXIndex index;
    CXTranslationUnit tu;
    CXPrintingPolicy policy;
    struct cm_tagname *tagnames;
    size_t ntagnames, captagnames;
    struct cm_site *site;
};

char *cm_take_string(CXString s);
const struct cm_file *cm_file_of(struct cm_state *st, CXFile f);
const struct cm_file *cm_cursor_file(struct cm_state *st, CXCursor c,
                                     unsigned *line, unsigned *offset);

/* clang_manifest_ast.c */
bool cm_walk(struct cm_state *st);

#endif /* ZCL_TOOLS_SENSORS_CLANG_MANIFEST_H */
