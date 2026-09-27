/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts fuzzer's fixed reproducers: each minimized project pair a differential run found (F<n>) or that pins a shape the consumer must keep planning safely (pass_*). */
#include "test/semantic_fuzz.h"

#include "base/log_macros.h"

#include <string.h>

/* The after text of a file the edit leaves alone. */
static const char k_sfz_same[] = "";
#define SFZ_SAME k_sfz_same

static const struct sfz_file k_f1_flag[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_mode(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         "# p\n"
         "CFLAGS_EXTRA = -DPROJ_MODE=1\n"},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_mode(void)\n"
         "{\n"
         "#ifdef PROJ_MODE\n"
         "    return 1;\n"
         "#else\n"
         "    return 2;\n"
         "#endif\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f2_counter_c[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_a(int x);\n"
         "int t0_b(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "    return x + 1;\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return __COUNTER__;\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "    return x + 1 + 0 * __COUNTER__;\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return __COUNTER__;\n"
         "}\n"},
};

static const struct sfz_file k_f2_counter_header[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static inline int h_inl(int x)\n"
         "{\n"
         "    return x + 1;\n"
         "}\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static inline int h_inl(int x)\n"
         "{\n"
         "    return x + 1 + 0 * __COUNTER__;\n"
         "}\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_inl(int x)\n"
         "{\n"
         "    return h_inl(x);\n"
         "}\n"
         "int t0_ctr(void)\n"
         "{\n"
         "    return __COUNTER__;\n"
         "}\n",
         SFZ_SAME},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_ctr(void)\n"
         "{\n"
         "    return __COUNTER__;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f3_line_c[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_a(int x);\n"
         "int t0_b(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "    return x + 1;\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return __LINE__;\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "\n"
         "    return x + 1;\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return __LINE__;\n"
         "}\n"},
};

static const struct sfz_file k_f4_hasinc_create_gccdeps[] = {
    {"inc1/opt.h",
         NULL,
         "/* optional feature */\n"},
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#if __has_include(\"opt.h\")\n"
         "#define H_V 1\n"
         "#else\n"
         "#define H_V 2\n"
         "#endif\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_f(void)\n"
         "{\n"
         "    return H_V + 10;\n"
         "}\n",
         SFZ_SAME},
    {"src/t1.c",
         "int t1_g(void)\n"
         "{\n"
         "    return 7;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f4_hasinc_delete[] = {
    {"inc1/opt.h",
         "/* optional feature */\n",
         NULL},
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#if __has_include(\"opt.h\")\n"
         "#define H_V 1\n"
         "#else\n"
         "#define H_V 2\n"
         "#endif\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_f(void)\n"
         "{\n"
         "    return H_V + 10;\n"
         "}\n",
         SFZ_SAME},
    {"src/t1.c",
         "int t1_g(void)\n"
         "{\n"
         "    return 7;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f5_header_static[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static int h_state;\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "void t0_set(void)\n"
         "{\n"
         "    h_state = 5;\n"
         "}\n"
         "int t0_get(void)\n"
         "{\n"
         "    return h_state;\n"
         "}\n",
         "#include \"h.h\"\n"
         "void t0_set(void)\n"
         "{\n"
         "    h_state = 6;\n"
         "}\n"
         "int t0_get(void)\n"
         "{\n"
         "    return h_state;\n"
         "}\n"},
};

static const struct sfz_file k_f6_alias[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int h_api(int x);\n"
         "int h_api_compat(int x);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int h_api(int x)\n"
         "{\n"
         "    return x * 3 + 1;\n"
         "}\n"
         "int h_api_compat(int x) __attribute__((alias(\"h_api\")));\n",
         "#include \"h.h\"\n"
         "int h_api(int x)\n"
         "{\n"
         "    return x * 3 + 2;\n"
         "}\n"
         "int h_api_compat(int x) __attribute__((alias(\"h_api\")));\n"},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_use(int x)\n"
         "{\n"
         "    return h_api_compat(x) + 2;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f7_cleanup_handler[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_work(int x);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_sink;\n"
         "static void t0_release(int *p)\n"
         "{\n"
         "    t0_sink = *p + 1;\n"
         "}\n"
         "int t0_work(int x)\n"
         "{\n"
         "    __attribute__((cleanup(t0_release))) int v = x * 2;\n"
         "    return v + 3;\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_sink;\n"
         "static void t0_release(int *p)\n"
         "{\n"
         "    t0_sink = *p + 7;\n"
         "}\n"
         "int t0_work(int x)\n"
         "{\n"
         "    __attribute__((cleanup(t0_release))) int v = x * 2;\n"
         "    return v + 3;\n"
         "}\n"},
};

/* F7 behind a same-name seed: t0's external work() inlines a cleanup()
 * handler whose body changed, and t1's own static work() changed too. A
 * seed matched by bare name would let t1's seed cover t0's work. */
static const struct sfz_file k_f7_cleanup_same_name[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t1_run(int x);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_sink;\n"
         "static void t0_release(int *p)\n"
         "{\n"
         "    t0_sink = *p + 1;\n"
         "}\n"
         "int work(int x)\n"
         "{\n"
         "    __attribute__((cleanup(t0_release))) int v = x * 2;\n"
         "    return v + 3;\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_sink;\n"
         "static void t0_release(int *p)\n"
         "{\n"
         "    t0_sink = *p + 7;\n"
         "}\n"
         "int work(int x)\n"
         "{\n"
         "    __attribute__((cleanup(t0_release))) int v = x * 2;\n"
         "    return v + 3;\n"
         "}\n"},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "static int work(int x)\n"
         "{\n"
         "    return x * 5 + 1;\n"
         "}\n"
         "int t1_run(int x)\n"
         "{\n"
         "    return work(x) + 2;\n"
         "}\n",
         "#include \"h.h\"\n"
         "static int work(int x)\n"
         "{\n"
         "    return x * 5 + 4;\n"
         "}\n"
         "int t1_run(int x)\n"
         "{\n"
         "    return work(x) + 2;\n"
         "}\n"},
};

static const struct sfz_file k_f8_c_includes_c[] = {
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/b.c",
         "static int b_k(int x)\n"
         "{\n"
         "    return x * 3 + 1;\n"
         "}\n"
         "int b_api(int x)\n"
         "{\n"
         "    return b_k(x);\n"
         "}\n",
         "static int b_k(int x)\n"
         "{\n"
         "    return x * 3 + 5;\n"
         "}\n"
         "int b_api(int x)\n"
         "{\n"
         "    return b_k(x);\n"
         "}\n"},
    {"src/test_b.c",
         "#include \"b.c\"\n"
         "int test_b_k(void)\n"
         "{\n"
         "    return b_k(4) == 13;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_f8_c_includes_c_renamed[] = {
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/a.c",
         "#define b_api a_b_api\n"
         "#include \"b.c\"\n"
         "#undef b_api\n"
         "int a_use(int x)\n"
         "{\n"
         "    return b_k(x) + 2;\n"
         "}\n",
         SFZ_SAME},
    {"src/b.c",
         "static int b_k(int x)\n"
         "{\n"
         "    return x * 3 + 1;\n"
         "}\n"
         "int b_api(int x)\n"
         "{\n"
         "    return b_k(x);\n"
         "}\n",
         "static int b_k(int x)\n"
         "{\n"
         "    return x * 3 + 5;\n"
         "}\n"
         "int b_api(int x)\n"
         "{\n"
         "    return b_k(x);\n"
         "}\n"},
};

static const struct sfz_file k_pass_hd[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_A 1\n"
         "#if H_A > 2\n"
         "#define H_D 100\n"
         "#else\n"
         "#define H_D 200\n"
         "#endif\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_A 3\n"
         "#if H_A > 2\n"
         "#define H_D 100\n"
         "#else\n"
         "#define H_D 200\n"
         "#endif\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_d(void)\n"
         "{\n"
         "    return H_D;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_pass_hdrproto[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static int t0_k(int a);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "static int t0_k(int a)\n"
         "{\n"
         "    return a * 3 + 7;\n"
         "}\n"
         "int t0_e(int x)\n"
         "{\n"
         "    return t0_k(x) + 1;\n"
         "}\n",
         "#include \"h.h\"\n"
         "static int t0_k(int a)\n"
         "{\n"
         "    return a * 3 + 8;\n"
         "}\n"
         "int t0_e(int x)\n"
         "{\n"
         "    return t0_k(x) + 1;\n"
         "}\n"},
};

static const struct sfz_file k_pass_undef[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_TMP 5\n"
         "static inline int h_tmp(void)\n"
         "{\n"
         "    return H_TMP;\n"
         "}\n"
         "#undef H_TMP\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_TMP 6\n"
         "static inline int h_tmp(void)\n"
         "{\n"
         "    return H_TMP;\n"
         "}\n"
         "#undef H_TMP\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_f(void)\n"
         "{\n"
         "    return h_tmp() + 1;\n"
         "}\n",
         SFZ_SAME},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_g(void)\n"
         "{\n"
         "    return 7;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_pass_undefbody[] = {
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#define T0_Q 3\n"
         "int t0_f(void)\n"
         "{\n"
         "    int r = T0_Q;\n"
         "    return r;\n"
         "}\n"
         "int t0_g(void)\n"
         "{\n"
         "#ifdef T0_Q\n"
         "    return 1;\n"
         "#else\n"
         "    return 2;\n"
         "#endif\n"
         "}\n",
         "#define T0_Q 3\n"
         "int t0_f(void)\n"
         "{\n"
         "    int r = T0_Q;\n"
         "#undef T0_Q\n"
         "    return r;\n"
         "}\n"
         "int t0_g(void)\n"
         "{\n"
         "#ifdef T0_Q\n"
         "    return 1;\n"
         "#else\n"
         "    return 2;\n"
         "#endif\n"
         "}\n"},
};

#define SFZ_FILES(a) a, sizeof(a) / sizeof((a)[0])

const struct sfz_repro k_sfz_repros[] = {
    {"F1_flag", "flag", "Makefile CFLAGS_EXTRA gains -DPROJ_MODE=1", false,
     NULL, SFZ_FILES(k_f1_flag), NULL},
    {"F2_counter_c", "counter_c",
     "t0_a gains a __COUNTER__ expansion; t0_b, below it and unchanged, now "
     "returns 1",
     false, NULL, SFZ_FILES(k_f2_counter_c), NULL},
    {"F2_counter_header", "header_inline",
     "h_inl gains one __COUNTER__ expansion; every later __COUNTER__ in each "
     "reader shifts by one",
     false, NULL, SFZ_FILES(k_f2_counter_header), NULL},
    {"F3_line_c", "comment_ws",
     "a blank line inside t0_a moves t0_b down one line; __LINE__ in t0_b "
     "changes",
     false, NULL, SFZ_FILES(k_f3_line_c), NULL},
    {"F4_hasinc_create_gccdeps", "shadow",
     "new inc1/opt.h flips a __has_include in inc2/h.h; gcc depfiles omit "
     "the probe",
     true, NULL, SFZ_FILES(k_f4_hasinc_create_gccdeps), NULL},
    {"F4_hasinc_delete", "hasinc",
     "deleting inc1/opt.h flips a __has_include in inc2/h.h", false, NULL,
     SFZ_FILES(k_f4_hasinc_delete), NULL},
    {"F5_header_static", "body_extern",
     "t0_set stores 6 not 5 into a header-defined static; t0_get folds it",
     false, NULL, SFZ_FILES(k_f5_header_static), NULL},
    {"F6_alias", "body_extern", "body of h_api, which h_api_compat aliases",
     false, NULL, SFZ_FILES(k_f6_alias), NULL},
    {"F7_cleanup_handler", "body_static",
     "body of a cleanup() handler that t0_work inlines", false,
     "the consumer's cleanup-handler fix: t0_work runs and inlines its "
     "__attribute__((cleanup)) handler t0_release, whose body changed, and "
     "is not a seed",
     SFZ_FILES(k_f7_cleanup_handler),
     "src/t0.c t0_work NOT-COVERED\n"},
    {"F7_cleanup_same_name", "body_static",
     "t0's work() inlines a cleanup() handler whose body changed; t1's own "
     "static work() changed too",
     false,
     "the consumer's cleanup-handler fix (as F7_cleanup_handler); a seed "
     "matched by bare name would hide it behind t1's static work",
     SFZ_FILES(k_f7_cleanup_same_name),
     "src/t0.c work NOT-COVERED\n"},
    {"F8_c_includes_c", "body_static",
     "test_b.c #includes b.c to reach its statics; b_k body changes", false,
     "the consumer's .c-includes-.c fix: test_b.c compiles b.c's statics "
     "into its own object, and a b.c edit does not seed the includer's "
     "functions that inline them (test_b_k)",
     SFZ_FILES(k_f8_c_includes_c),
     "src/test_b.c test_b_k NOT-COVERED\n"},
    {"F8_c_includes_c_renamed", "body_static",
     "a.c #includes b.c (unity build); b_k body changes", false,
     "the consumer's .c-includes-.c fix: a.c compiles b.c's functions into "
     "its own object, and a b.c edit does not seed the includer's functions "
     "that inline them (a_b_api, a_use)",
     SFZ_FILES(k_f8_c_includes_c_renamed),
     "src/a.c a_b_api NOT-COVERED\n"
     "src/a.c a_use NOT-COVERED\n"},
    {"pass_hd", "macro_value", "H_A 1->3 flips H_D", false, NULL,
     SFZ_FILES(k_pass_hd), NULL},
    {"pass_hdrproto", "body_static",
     "body of a static declared in a header, defined in the main file", false,
     NULL, SFZ_FILES(k_pass_hdrproto), NULL},
    {"pass_undef", "macro_value", "H_TMP 5->6, #undef after use", false, NULL,
     SFZ_FILES(k_pass_undef), NULL},
    {"pass_undefbody", "body_extern", "#undef inside t0_f changes #ifdef in t0_g",
     false, NULL, SFZ_FILES(k_pass_undefbody), NULL},
};
const size_t k_sfz_nrepros = sizeof(k_sfz_repros) / sizeof(k_sfz_repros[0]);

static bool write_side(const struct sfz_repro *r, const char *dir, bool after)
{
    for (size_t k = 0; k < r->nfiles; k++) {
        const struct sfz_file *f = &r->files[k];
        const char *text = after && f->after != SFZ_SAME ? f->after : f->before;
        if (text != NULL && !sfz_put(dir, f->path, text, strlen(text)))
            LOG_FAIL("sfz", "%s: cannot write %s/%s", r->name, dir, f->path);
    }
    /* keep both -I dirs present, as the generator does */
    return sfz_put(dir, "inc1/.keep", "keep\n", 5) &&
           sfz_put(dir, "inc2/.keep", "keep\n", 5);
}

bool sfz_write_repro(const struct sfz_repro *r, const char *dir)
{
    char side[4096];
    if (snprintf(side, sizeof(side), "%s/before", dir) >= (int)sizeof(side))
        LOG_FAIL("sfz", "path too long: %s", dir);
    if (!write_side(r, side, false))
        return false;
    (void)snprintf(side, sizeof(side), "%s/after", dir);
    return write_side(r, side, true);
}
