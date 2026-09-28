/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts fuzzer's fixed reproducers: each minimized project pair a differential run found (F<n>) or that pins a shape the consumer must keep planning safely (pass_*, and D<n> for edits that change only data a function addresses or an object defines). */
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

/* The data shapes: each edit changes only data a function addresses
 * through a relocation, or data the object defines. */

static const struct sfz_file k_d1_string_literal[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "const char *t0_name(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "const char *t0_name(void)\n"
         "{\n"
         "    return \"alpha\";\n"
         "}\n"
         "const char *t0_tail(void)\n"
         "{\n"
         "    return \"omega\";\n"
         "}\n",
         "#include \"h.h\"\n"
         "const char *t0_name(void)\n"
         "{\n"
         "    return \"alphabet\";\n"
         "}\n"
         "const char *t0_tail(void)\n"
         "{\n"
         "    return \"omega\";\n"
         "}\n"},
};

static const struct sfz_file k_d2_static_const_table[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_pick(int i);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "static const int t0_tab[4] = {3, 5, 7, 9};\n"
         "int t0_pick(int i)\n"
         "{\n"
         "    return t0_tab[i & 3];\n"
         "}\n",
         "#include \"h.h\"\n"
         "static const int t0_tab[4] = {3, 5, 8, 9};\n"
         "int t0_pick(int i)\n"
         "{\n"
         "    return t0_tab[i & 3];\n"
         "}\n"},
};

static const struct sfz_file k_d3_header_const_table[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static const int h_ctab[3] = {10, 20, 30};\n"
         "int t0_get(int i);\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "static const int h_ctab[3] = {10, 21, 30};\n"
         "int t0_get(int i);\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_get(int i)\n"
         "{\n"
         "    return h_ctab[i % 3];\n"
         "}\n",
         SFZ_SAME},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_get(int i)\n"
         "{\n"
         "    return t0_get(i) + h_ctab[i & 1];\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_d4_header_index_macro[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_IDX 1\n"
         "extern int h_arr[4];\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_IDX 2\n"
         "extern int h_arr[4];\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int h_arr[4] = {2, 4, 6, 8};\n",
         SFZ_SAME},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_at(void)\n"
         "{\n"
         "    return h_arr[H_IDX];\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_d5_extern_const_init[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_LIMIT 40\n"
         "extern const int h_limit;\n"
         "#endif\n",
         "#ifndef H_H\n"
         "#define H_H\n"
         "#define H_LIMIT 41\n"
         "extern const int h_limit;\n"
         "#endif\n"},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "const int h_limit = H_LIMIT;\n",
         SFZ_SAME},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_lim(void)\n"
         "{\n"
         "    return h_limit + 1;\n"
         "}\n",
         SFZ_SAME},
};

static const struct sfz_file k_d6_function_static_table[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_pick(int i);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile",
         "# p\n"
         "CFLAGS_EXTRA = \n",
         SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_pick(int i)\n"
         "{\n"
         "    static const int tab[4] = {3, 5, 7, 9};\n"
         "    return tab[i & 3];\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_pick(int i)\n"
         "{\n"
         "    static const int tab[4] = {3, 5, 8, 9};\n"
         "    return tab[i & 3];\n"
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
     "body of a cleanup() handler that t0_work inlines (fixed by f6b50b90fd: "
     "the handler's runner is seeded)",
     false, NULL, SFZ_FILES(k_f7_cleanup_handler), NULL},
    {"F7_cleanup_same_name", "body_static",
     "t0's work() inlines a cleanup() handler whose body changed; t1's own "
     "static work() changed too; a seed matched by bare name would hide a "
     "miss behind t1's static work",
     false, NULL, SFZ_FILES(k_f7_cleanup_same_name), NULL},
    {"F8_c_includes_c", "body_static",
     "test_b.c #includes b.c to reach its statics; b_k body changes (fixed "
     "by f6b50b90fd: the includer's functions are seeded)",
     false, NULL, SFZ_FILES(k_f8_c_includes_c), NULL},
    {"F8_c_includes_c_renamed", "body_static",
     "a.c #includes b.c (unity build); b_k body changes; a_b_api and a_use "
     "are seeded",
     false, NULL, SFZ_FILES(k_f8_c_includes_c_renamed), NULL},
    {"pass_hd", "macro_value", "H_A 1->3 flips H_D", false, NULL,
     SFZ_FILES(k_pass_hd), NULL},
    {"pass_hdrproto", "body_static",
     "body of a static declared in a header, defined in the main file", false,
     NULL, SFZ_FILES(k_pass_hdrproto), NULL},
    {"pass_undef", "macro_value", "H_TMP 5->6, #undef after use", false, NULL,
     SFZ_FILES(k_pass_undef), NULL},
    {"pass_undefbody", "body_extern", "#undef inside t0_f changes #ifdef in t0_g",
     false, NULL, SFZ_FILES(k_pass_undefbody), NULL},
    {"D1_string_literal", "data_string",
     "t0_name returns a longer literal; t0_tail's literal moves in the "
     "merged string section",
     false, NULL, SFZ_FILES(k_d1_string_literal), NULL},
    {"D2_static_const_table", "data_table",
     "one entry of a file-scope static const table that t0_pick indexes",
     false, NULL, SFZ_FILES(k_d2_static_const_table), NULL},
    {"D3_header_const_table", "data_hconst",
     "one entry of a header static const table that t0 and t1 index",
     false, NULL, SFZ_FILES(k_d3_header_const_table), NULL},
    {"D4_header_index_macro", "data_index",
     "H_IDX 1->2 moves t1_at's read of the global array t0 defines", false,
     NULL, SFZ_FILES(k_d4_header_index_macro), NULL},
    {"D5_extern_const_init", "header_const",
     "H_LIMIT 40->41 initializes the global const t0 defines, which t1 reads",
     false, NULL, SFZ_FILES(k_d5_extern_const_init), NULL},
    {"D6_function_static_table", "data_table",
     "one entry of a static const table inside t0_pick", false, NULL,
     SFZ_FILES(k_d6_function_static_table), NULL},
};
const size_t k_sfz_nrepros = sizeof(k_sfz_repros) / sizeof(k_sfz_repros[0]);

/* ---- toolchain reproducers: the compiler or its optimizer, not the text ---- */

/* A comment inside t0_a; t1.c is untouched. Built with another compiler
 * after the edit, every object changes while every manifest stays the
 * same: the sensor records its own libclang version and the argv, never
 * the compiler that builds the object. */
static const struct sfz_file k_f9_cc_drift[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_a(int x);\n"
         "int t1_a(int x);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile", "# p\nCFLAGS_EXTRA = \n", SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "    return x * 3 + 1;\n"
         "}\n",
         "#include \"h.h\"\n"
         "int t0_a(int x)\n"
         "{\n"
         "    /* scaled */\n"
         "    return x * 3 + 1;\n"
         "}\n"},
    {"src/t1.c",
         "#include \"h.h\"\n"
         "int t1_a(int x)\n"
         "{\n"
         "    return x * 5 + 2;\n"
         "}\n",
         SFZ_SAME},
};

/* t0_f passes 8, not 7, to a noinline static. gcc reads "-O02" and
 * "--optimize=2" as -O2 and specializes the static for its one caller
 * (t0_s.constprop.0); the consumer reads -O02 as -O0 and --optimize=2 as
 * no optimizer, and seeds only t0_f and its callers. */
static const struct sfz_file k_f10_opt_spelling[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_f(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile", "# p\nCFLAGS_EXTRA = \n", SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "static __attribute__((noinline)) int t0_s(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x;\n"
         "    return r;\n"
         "}\n"
         "int t0_f(void)\n"
         "{\n"
         "    return t0_s(7);\n"
         "}\n",
         "#include \"h.h\"\n"
         "static __attribute__((noinline)) int t0_s(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x;\n"
         "    return r;\n"
         "}\n"
         "int t0_f(void)\n"
         "{\n"
         "    return t0_s(8);\n"
         "}\n"},
};

/* t0_a passes 8, not 7, to an external noinline function t0_b also calls.
 * gcc at -O3 (and -O5, which it reads as -O3) clones the callee for each
 * constant (t0_w.constprop.N); the consumer reads -O5 as the -O1
 * component model, which never seeds an external callee. */
static const struct sfz_file k_f10_extern_clone[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_w(int x);\n"
         "int t0_a(void);\n"
         "int t0_b(void);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile", "# p\nCFLAGS_EXTRA = \n", SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "__attribute__((noinline)) int t0_w(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x;\n"
         "    return r;\n"
         "}\n"
         "int t0_a(void)\n"
         "{\n"
         "    return t0_w(7);\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return t0_w(7);\n"
         "}\n",
         "#include \"h.h\"\n"
         "__attribute__((noinline)) int t0_w(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x;\n"
         "    return r;\n"
         "}\n"
         "int t0_a(void)\n"
         "{\n"
         "    return t0_w(8);\n"
         "}\n"
         "int t0_b(void)\n"
         "{\n"
         "    return t0_w(7);\n"
         "}\n"},
};

/* t0_p's body becomes t0_q's. At -O2 gcc folds the identical statics
 * (ipa-icf), so t0_q now names t0_p's bytes; the manifest says -Og, as
 * the facts rule's argv does for the dev build's -O2 hot objects, and the
 * component model never relates t0_q to t0_p. */
static const struct sfz_file k_f11_hot_icf[] = {
    {"inc2/h.h",
         "#ifndef H_H\n"
         "#define H_H\n"
         "int t0_ep(int x);\n"
         "int t0_eq(int x);\n"
         "#endif\n",
         SFZ_SAME},
    {"Makefile", "# p\nCFLAGS_EXTRA = \n", SFZ_SAME},
    {"src/t0.c",
         "#include \"h.h\"\n"
         "static __attribute__((noinline)) int t0_p(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x + 3;\n"
         "    return r;\n"
         "}\n"
         "static __attribute__((noinline)) int t0_q(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x + 5;\n"
         "    return r;\n"
         "}\n"
         "int t0_ep(int x)\n"
         "{\n"
         "    return t0_p(x) + 1;\n"
         "}\n"
         "int t0_eq(int x)\n"
         "{\n"
         "    return t0_q(x) + 2;\n"
         "}\n",
         "#include \"h.h\"\n"
         "static __attribute__((noinline)) int t0_p(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x + 5;\n"
         "    return r;\n"
         "}\n"
         "static __attribute__((noinline)) int t0_q(int x)\n"
         "{\n"
         "    int r = 0;\n"
         "    for (int i = 0; i < x; i++)\n"
         "        r += i * x + 5;\n"
         "    return r;\n"
         "}\n"
         "int t0_ep(int x)\n"
         "{\n"
         "    return t0_p(x) + 1;\n"
         "}\n"
         "int t0_eq(int x)\n"
         "{\n"
         "    return t0_q(x) + 2;\n"
         "}\n"},
};


const struct sfz_tool_repro k_sfz_tool_repros[] = {
    {.r = {.name = "F9_cc_drift", .kind = "comment_ws",
           .detail = "a comment in t0_a; the object compiler changes from "
                     "clang to gcc between the sides, the argv does not",
           .known_red = "tool-drift",
           .files = SFZ_FILES(k_f9_cc_drift),
           .known_red_why = "?"},
     .cc_before = "clang", .cc_after = "gcc"},
    {.r = {.name = "F10_opt_spelling_O02", .kind = "body_extern",
           .detail = "t0_f passes 8, not 7, to a noinline static; gcc at -O02",
           .known_red = "opt-spelling",
           .files = SFZ_FILES(k_f10_opt_spelling),
           .known_red_why = "?"},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O02"},
    {.r = {.name = "F10_opt_spelling_long", .kind = "body_extern",
           .detail = "t0_f passes 8, not 7, to a noinline static; gcc at "
                     "--optimize=2",
           .known_red = "opt-spelling",
           .files = SFZ_FILES(k_f10_opt_spelling),
           .known_red_why = "?"},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "--optimize=2"},
    {.r = {.name = "pass_opt_O1_gcc", .kind = "body_extern",
           .detail = "t0_f passes 8, not 7, to a noinline static; gcc at -O1",
           .files = SFZ_FILES(k_f10_opt_spelling)},
     .cc_before = "gcc", .cc_after = "gcc"},
    {.r = {.name = "pass_opt_O2_gcc", .kind = "body_extern",
           .detail = "t0_f passes 8, not 7, to a noinline static; gcc at -O2",
           .files = SFZ_FILES(k_f10_opt_spelling)},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O2"},
    {.r = {.name = "F10_opt_spelling_O5", .kind = "body_extern",
           .detail = "t0_a passes 8, not 7, to an external noinline t0_w; "
                     "gcc at -O5",
           .known_red = "opt-spelling",
           .files = SFZ_FILES(k_f10_extern_clone),
           .known_red_why = "?"},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O5"},
    {.r = {.name = "pass_extern_clone_O3", .kind = "body_extern",
           .detail = "t0_a passes 8, not 7, to an external noinline t0_w; "
                     "gcc at -O3",
           .files = SFZ_FILES(k_f10_extern_clone)},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O3"},
    {.r = {.name = "F11_hot_icf", .kind = "body_static",
           .detail = "t0_p's body becomes t0_q's; objects at gcc -O2, the "
                     "sensor told -Og",
           .known_red = "hot-identity",
           .files = SFZ_FILES(k_f11_hot_icf),
           .known_red_why = "?"},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O2/-Og"},
    {.r = {.name = "pass_hot_icf_O2", .kind = "body_static",
           .detail = "t0_p's body becomes t0_q's; objects and sensor at gcc "
                     "-O2",
           .files = SFZ_FILES(k_f11_hot_icf)},
     .cc_before = "gcc", .cc_after = "gcc", .opt = "-O2"},
};
const size_t k_sfz_ntool_repros =
    sizeof(k_sfz_tool_repros) / sizeof(k_sfz_tool_repros[0]);

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
