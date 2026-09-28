/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * z23-sem-replay, proved on a history whose answer is known.
 *
 * The replay is a reporting tool: it replays real commits through make's
 * test-fast object build, the semantic sensor and dev.change.plan, and
 * compares what make recompiled with what the facts plan would have
 * compiled. Its one hard verdict is exit 3 on a code false negative (too
 * narrow), an object whose code changed that the facts plan left out; it
 * also counts false-wide TUs a plan selected whose object bytes did not
 * move, which is an over-selection, not a build-breaking miss, so it never
 * stops the run. A replay that missed a false negative, or that miscounted
 * either direction, would report savings that are not there. So this group
 * runs the REAL built binary (build/bin/z23-sem-replay) as a subprocess
 * against a throwaway git repository it builds itself:
 *
 *   P   src/a.c, src/b.c (both include src/common.h) and src/c.c (main)
 *   C1  changes the body of src/a.c
 *   C2  changes the inline function in src/common.h that only a.c calls
 *
 * The fixture's Makefile has the shape the replay reads from the real one:
 * TEST_PARALLEL_FAST_CANDIDATE names the link, build/test-obj/.current-epoch
 * names the epoch directory, and each object is compiled by one recipe line
 * "<tool> dep OBJ SRC -- CC FLAGS" (a small compiled wrapper that writes the
 * depfile and publishes the object by rename, as the real epoch compiler
 * does). The sensor and the planner are small compiled stand-ins: the sensor
 * writes the --cc and --toolchain-id it was given, then the source, into its
 * --out manifest (the replay only hashes manifests), and the planner
 * prints a canned dev.change.plan reply read from files beside it, with the
 * facts reply chosen when --input names "facts".
 *
 *   step C1 with a facts reply that lists no TU: make rebuilt src/a.c and its
 *   code changed, so the step must exit SR_STEP_FALSE_NEGATIVE (3) and write
 *   the TU to MISSES.tsv and sets.tsv. `run` over the same commit, with the
 *   tool found on PATH by its bare name, re-executes itself for the step and
 *   stops with the same exit.
 *
 *   step C2 with a facts reply that narrows to src/a.c: make rebuilds a.o and
 *   b.o, only a.o's bytes change, the plain plan selects both TUs and two
 *   groups, the facts plan one TU and one group. Every count is exact, and
 *   the report headline says 1 compile and 1 group avoided, with false-wide
 *   vs plain 1 (b.o was recompiled byte-identical, so the plain set's b.c
 *   is a natural over-selection) and false-wide vs facts 0.
 *
 *   step C2 again with a facts reply that over-selects src/b.c (the planted
 *   false-wide): the step still exits 0 (over-selection never stops a run),
 *   and its false-wide counts are exact: facts 1, plain 1.
 *
 * Everything runs under test-tmp/. git and the replay run under `env -u`
 * for the make and git variables a surrounding `make` or hook exports, so
 * the fixture's make and git never see the outer build or checkout. */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE /* realpath */
#endif

#include "test/test_core.h"

int test_sem_replay(void);

#if !defined(_WIN32)

#include "util/spawn.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SRT_EPOCH "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
/* The fixture build's toolchain identity, as make's BUILD_COMPILER_ID. */
#define SRT_TOOLCHAIN "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
#define SRT_TIMEOUT_MS 600000
#define SRT_EXIT_FALSE_NEGATIVE 3 /* SR_STEP_FALSE_NEGATIVE, sem_replay_step.h */

struct srt_fx {
    char root[PATH_MAX];  /* test-tmp/sem_replay_<pid>_fx */
    char repo[PATH_MAX];  /* the fixture git repository */
    char tools[PATH_MAX]; /* compiled fixture programs */
    char tool[PATH_MAX];  /* the real z23-sem-replay */
    char p[64], c1[64], c2[64];
    bool objcopy;
};

static char g_srt_out[1 << 16];

/* ── the fixture programs ────────────────────────────────────────────── */

/* "<tool> dep OBJ SRC -- CC FLAGS...": compile SRC to OBJ.tmp with its
 * depfile beside OBJ, then publish OBJ by rename. */
static const char k_epoch_object[] =
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "#include <sys/wait.h>\n"
    "#include <unistd.h>\n"
    "int main(int argc, char **argv)\n"
    "{\n"
    "    char tmp[4096], dep[4096], *av[256];\n"
    "    int n = 0, st = 0;\n"
    "    if (argc < 6 || argc > 200 || strcmp(argv[1], \"dep\") != 0 ||\n"
    "        strcmp(argv[4], \"--\") != 0)\n"
    "        return 2;\n"
    "    size_t ol = strlen(argv[2]);\n"
    "    if (ol < 3 || strcmp(argv[2] + ol - 2, \".o\") != 0)\n"
    "        return 2;\n"
    "    snprintf(tmp, sizeof tmp, \"%s.tmp\", argv[2]);\n"
    "    snprintf(dep, sizeof dep, \"%.*s.d\", (int)(ol - 2), argv[2]);\n"
    "    for (int i = 5; i < argc; i++)\n"
    "        av[n++] = argv[i];\n"
    "    const char *tail[] = {\"-c\", argv[3], \"-o\", tmp, \"-MMD\", \"-MP\",\n"
    "                          \"-MF\", dep, \"-MT\", argv[2]};\n"
    "    for (int i = 0; i < 10; i++)\n"
    "        av[n++] = (char *)tail[i];\n"
    "    av[n] = NULL;\n"
    "    pid_t pid = fork();\n"
    "    if (pid == 0) {\n"
    "        execvp(av[0], av);\n"
    "        _exit(127);\n"
    "    }\n"
    "    if (pid < 0 || waitpid(pid, &st, 0) < 0 || !WIFEXITED(st) ||\n"
    "        WEXITSTATUS(st) != 0)\n"
    "        return 1;\n"
    "    return rename(tmp, argv[2]) == 0 ? 0 : 1;\n"
    "}\n";

/* "emit --root R --source TU --out F --facts [--toolchain-id X] [--cc Y]
 * -- FLAGS": F gets "cc=Y toolchain=X" (a dash for one not given) on its
 * first line, then TU's bytes. */
static const char k_sensor[] =
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "int main(int argc, char **argv)\n"
    "{\n"
    "    const char *src = NULL, *out = NULL, *cc = \"-\", *tc = \"-\";\n"
    "    char buf[4096];\n"
    "    size_t n;\n"
    "    if (argc < 2 || strcmp(argv[1], \"emit\") != 0)\n"
    "        return 2;\n"
    "    for (int i = 2; i + 1 < argc && strcmp(argv[i], \"--\") != 0; i++) {\n"
    "        if (strcmp(argv[i], \"--source\") == 0)\n"
    "            src = argv[++i];\n"
    "        else if (strcmp(argv[i], \"--out\") == 0)\n"
    "            out = argv[++i];\n"
    "        else if (strcmp(argv[i], \"--cc\") == 0)\n"
    "            cc = argv[++i];\n"
    "        else if (strcmp(argv[i], \"--toolchain-id\") == 0)\n"
    "            tc = argv[++i];\n"
    "        else if (strcmp(argv[i], \"--root\") == 0)\n"
    "            i++;\n"
    "    }\n"
    "    FILE *in = src ? fopen(src, \"rb\") : NULL;\n"
    "    FILE *o = in && out ? fopen(out, \"wb\") : NULL;\n"
    "    if (o == NULL)\n"
    "        return 1;\n"
    "    fprintf(o, \"cc=%s toolchain=%s\\n\", cc, tc);\n"
    "    while ((n = fread(buf, 1, sizeof buf, in)) > 0)\n"
    "        fwrite(buf, 1, n, o);\n"
    "    fclose(in);\n"
    "    return fclose(o) == 0 ? 0 : 1;\n"
    "}\n";

/* "dev change plan --input=JSON": print <argv0>.facts.json when the request
 * names a facts directory, else <argv0>.plain.json. */
static const char k_planner[] =
    "#include <stdio.h>\n"
    "#include <string.h>\n"
    "int main(int argc, char **argv)\n"
    "{\n"
    "    char path[4096], buf[4096];\n"
    "    size_t n;\n"
    "    if (argc != 5 || strcmp(argv[1], \"dev\") != 0 ||\n"
    "        strcmp(argv[2], \"change\") != 0 || strcmp(argv[3], \"plan\") != 0 ||\n"
    "        strncmp(argv[4], \"--input={\\\"files\\\":[\", 18) != 0) {\n"
    "        fprintf(stderr, \"fixture planner: unexpected argv\\n\");\n"
    "        return 2;\n"
    "    }\n"
    "    snprintf(path, sizeof path, \"%s.%s.json\", argv[0],\n"
    "             strstr(argv[4], \",\\\"facts\\\":\\\"\") ? \"facts\" : \"plain\");\n"
    "    FILE *in = fopen(path, \"rb\");\n"
    "    if (in == NULL)\n"
    "        return 1;\n"
    "    while ((n = fread(buf, 1, sizeof buf, in)) > 0)\n"
    "        fwrite(buf, 1, n, stdout);\n"
    "    fclose(in);\n"
    "    return 0;\n"
    "}\n";

/* ── the canned dev.change.plan replies ──────────────────────────────── */

#define SRT_ENVELOPE                                                         \
    "{\"schema\":\"zcl.command_result.v1\",\"command\":\"dev.change.plan\","   \
    "\"ok\":true,\"status\":\"passed\",\"data_schema\":\"zcl.dev_change_plan.v1\"," \
    "\"data\":{\"closure_universal\":false,\"execution_selector\":\"exact\","

/* The plain plan: two groups. */
static const char k_plain_reply[] =
    SRT_ENVELOPE
    "\"execution_groups\":[\"test_fx_a\",\"test_fx_b\"],\"execution_groups_listed\":2,"
    "\"execution_groups_total\":2}}\n";

#define SRT_FACTS_HEAD                                                       \
    SRT_ENVELOPE                                                             \
    "\"execution_groups\":[\"test_fx_a\"],\"execution_groups_listed\":1,"     \
    "\"execution_groups_total\":1,\"facts\":{\"narrowed\":true,"

#define SRT_OBLIGATIONS                                                      \
    "\"obligations\":{\"reason\":\"fixture\",\"plain\":2,"                     \
    "\"plain_universal\":false,\"facts\":1,\"groups\":[{\"group\":"           \
    "\"test_fx_a\",\"reason\":\"fixture\"}],\"groups_listed\":1},"

/* A complete universe that lists no TU: the planted omission. */
static const char k_facts_omits[] =
    SRT_FACTS_HEAD
    "\"reason\":\"fixture-omits-the-changed-tu\",\"detail\":\"\",\"seeds\":[],"
    "\"seeds_total\":0,\"reached_files\":0,\"consumer\":\"fixture\","
    SRT_OBLIGATIONS
    "\"universe\":{\"applied\":true,\"complete\":true,\"reason\":\"\","
    "\"detail\":\"\",\"total\":0,\"affected\":0,\"offset\":0,\"listed\":0,"
    "\"next_offset\":null},\"tus\":[]}}}\n";

/* A complete universe that narrows to src/a.c. */
static const char k_facts_narrows[] =
    SRT_FACTS_HEAD
    "\"reason\":\"fixture-narrows\",\"detail\":\"\",\"seeds\":[\"fx_a_value\"],"
    "\"seeds_total\":1,\"reached_files\":1,\"consumer\":\"fixture\","
    SRT_OBLIGATIONS
    "\"universe\":{\"applied\":true,\"complete\":true,\"reason\":\"\","
    "\"detail\":\"\",\"total\":2,\"affected\":1,\"offset\":0,\"listed\":2,"
    "\"next_offset\":null},\"tus\":["
    "{\"path\":\"src/a.c\",\"affected\":true,\"broadened\":false,"
    "\"reason\":\"implementation-changed\",\"detail\":\"\"},"
    "{\"path\":\"src/b.c\",\"affected\":false,\"broadened\":false,"
    "\"reason\":\"unchanged\",\"detail\":\"\"}]}}}\n";

/* A complete universe that over-selects: both TUs affected, though only
 * src/a.c's object bytes actually change (the planted false-wide). */
static const char k_facts_overselects[] =
    SRT_FACTS_HEAD
    "\"reason\":\"fixture-overselects\",\"detail\":\"\",\"seeds\":[\"fx_a_value\"],"
    "\"seeds_total\":1,\"reached_files\":1,\"consumer\":\"fixture\","
    SRT_OBLIGATIONS
    "\"universe\":{\"applied\":true,\"complete\":true,\"reason\":\"\","
    "\"detail\":\"\",\"total\":2,\"affected\":2,\"offset\":0,\"listed\":2,"
    "\"next_offset\":null},\"tus\":["
    "{\"path\":\"src/a.c\",\"affected\":true,\"broadened\":false,"
    "\"reason\":\"implementation-changed\",\"detail\":\"\"},"
    "{\"path\":\"src/b.c\",\"affected\":true,\"broadened\":false,"
    "\"reason\":\"fixture-overselect\",\"detail\":\"\"}]}}}\n";

/* ── the fixture tree ────────────────────────────────────────────────── */

static const char k_header_p[] =
    "#ifndef FX_COMMON_H\n"
    "#define FX_COMMON_H\n"
    "static inline int fx_a_value(void) { return 1; }\n"
    "static inline int fx_b_value(void) { return 7; }\n"
    "#endif\n";

static const char k_header_c2[] =
    "#ifndef FX_COMMON_H\n"
    "#define FX_COMMON_H\n"
    "static inline int fx_a_value(void) { return 2; }\n"
    "static inline int fx_b_value(void) { return 7; }\n"
    "#endif\n";

static const char k_a_p[] =
    "#include \"common.h\"\n"
    "int fx_a(void);\n"
    "int fx_a(void) { return fx_a_value(); }\n";

static const char k_a_c1[] =
    "#include \"common.h\"\n"
    "int fx_a(void);\n"
    "int fx_a(void) { return fx_a_value() + 40; }\n";

static const char k_b[] =
    "#include \"common.h\"\n"
    "int fx_b(void);\n"
    "int fx_b(void) { return fx_b_value(); }\n";

static const char k_c[] =
    "int fx_a(void);\n"
    "int fx_b(void);\n"
    "int main(void) { return fx_a() + fx_b() > 0 ? 0 : 1; }\n";

/* Everything after the FX_TOOL line. */
static const char k_makefile_body[] =
    "FX_CC := cc\n"
    "FX_CFLAGS := -O1\n"
    "FX_EPOCH := " SRT_EPOCH "\n"
    "BUILD_COMPILER_ID := " SRT_TOOLCHAIN "\n"
    "FX_OBJ := build/test-obj/epochs/$(FX_EPOCH)\n"
    "FX_MARK := build/test-obj/.current-epoch\n"
    "TEST_PARALLEL_FAST_CANDIDATE := build/fixture-test\n"
    "FX_OBJS := $(patsubst %.c,$(FX_OBJ)/%.o,src/a.c src/b.c src/c.c)\n"
    "\n"
    "$(TEST_PARALLEL_FAST_CANDIDATE): $(FX_OBJS)\n"
    "\t$(FX_CC) -o $@ $(FX_OBJS)\n"
    "\n"
    "$(FX_MARK):\n"
    "\t@mkdir -p $(dir $@)\n"
    "\t@echo $(FX_EPOCH) > $@\n"
    "\n"
    "$(FX_OBJ)/%.o: %.c | $(FX_MARK)\n"
    "\t@mkdir -p $(dir $@)\n"
    "\t$(FX_TOOL) dep $@ $< -- $(FX_CC) $(FX_CFLAGS)\n"
    "\n"
    "-include $(FX_OBJS:.o=.d)\n";

/* ── process helpers ─────────────────────────────────────────────────── */

/* Variables a surrounding make or git hook exports that must not reach the
 * fixture's make and git. */
static const char *const k_env_clear[] = {
    "env", "-u", "MAKEFLAGS", "-u", "MFLAGS", "-u", "MAKELEVEL", "-u",
    "MAKEOVERRIDES", "-u", "GNUMAKEFLAGS", "-u", "GIT_DIR", "-u",
    "GIT_WORK_TREE", "-u", "GIT_INDEX_FILE", "-u", "GIT_COMMON_DIR", "-u",
    "GIT_OBJECT_DIRECTORY", "-u", "GIT_ALTERNATE_OBJECT_DIRECTORIES", "-u",
    "GIT_PREFIX", NULL};

/* Run args (NULL-terminated) under k_env_clear; stdout and stderr land in
 * g_srt_out. Returns the exit status, or -1. */
static int srt_run(const char *const *args)
{
    const char *argv[64];
    size_t n = 0;
    bool timed_out = false;
    for (size_t i = 0; k_env_clear[i] && n < 62; i++)
        argv[n++] = k_env_clear[i];
    for (size_t i = 0; args[i] && n < 63; i++)
        argv[n++] = args[i];
    argv[n] = NULL;
    int rc = zcl_spawn_capture_merged_observed(argv, g_srt_out, sizeof g_srt_out,
                                               SRT_TIMEOUT_MS, &timed_out);
    return timed_out ? -1 : rc;
}

static int srt_git(const char *repo, const char *const *args)
{
    const char *argv[24] = {"git", "-C", repo};
    size_t n = 3;
    for (size_t i = 0; args[i] && n < 23; i++)
        argv[n++] = args[i];
    argv[n] = NULL;
    return srt_run(argv);
}

static bool srt_mkdir_p(const char *path)
{
    char buf[PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof buf)
        return false;
    memcpy(buf, path, len + 1);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(buf, 0700) != 0 && errno != EEXIST)
            return false;
        *p = '/';
    }
    return mkdir(buf, 0700) == 0 || errno == EEXIST;
}

/* Write text to dir/rel, creating rel's directories. */
static bool srt_write(const char *dir, const char *rel, const char *text)
{
    char path[PATH_MAX];
    if (snprintf(path, sizeof path, "%s/%s", dir, rel) >= (int)sizeof path)
        return false;
    char *slash = strrchr(path, '/');
    *slash = '\0';
    bool ok = srt_mkdir_p(path);
    *slash = '/';
    FILE *f = ok ? fopen(path, "wb") : NULL;
    if (f == NULL)
        return false;
    size_t len = strlen(text);
    bool wrote = fwrite(text, 1, len, f) == len;
    return fclose(f) == 0 && wrote;
}

static bool srt_read(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    size_t n = fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
    return n < cap - 1;
}

/* ── building the fixture ────────────────────────────────────────────── */

/* Compile tools/<name>.c (text) to tools/<name>. */
static bool srt_cc(const struct srt_fx *fx, const char *name, const char *text)
{
    char src[PATH_MAX + 8], bin[PATH_MAX];
    char rel[64];
    snprintf(rel, sizeof rel, "%s.c", name);
    if (!srt_write(fx->tools, rel, text) ||
        snprintf(src, sizeof src, "%s/%s.c", fx->tools, name) >= (int)sizeof src ||
        snprintf(bin, sizeof bin, "%s/%s", fx->tools, name) >= (int)sizeof bin)
        return false;
    const char *argv[] = {"cc", "-std=c23", "-O1", "-o", bin, src, NULL};
    int rc = srt_run(argv);
    if (rc != 0)
        printf("(cc %s exited %d: %s) ", name, rc, g_srt_out);
    return rc == 0;
}

/* A planner stand-in named name whose replies are plain and facts. */
static bool srt_planner(const struct srt_fx *fx, const char *name, const char *facts)
{
    char rel[128];
    snprintf(rel, sizeof rel, "%s.plain.json", name);
    bool ok = srt_cc(fx, name, k_planner) && srt_write(fx->tools, rel, k_plain_reply);
    snprintf(rel, sizeof rel, "%s.facts.json", name);
    return ok && srt_write(fx->tools, rel, facts);
}

static bool srt_tools(struct srt_fx *fx)
{
    return srt_cc(fx, "fx-epoch-object", k_epoch_object) && srt_cc(fx, "fx-sensor", k_sensor) &&
           srt_planner(fx, "fx-planner-omits", k_facts_omits) &&
           srt_planner(fx, "fx-planner-narrows", k_facts_narrows) &&
           srt_planner(fx, "fx-planner-overselects", k_facts_overselects);
}

/* Stage everything under the repo and commit it; out gets the commit. */
static bool srt_commit(const struct srt_fx *fx, const char *msg, char out[64])
{
    const char *add[] = {"add", "-A", NULL};
    const char *commit[] = {"-c", "user.name=sem-replay", "-c",
                            "user.email=sem-replay@z23.invalid", "commit", "--quiet",
                            "--no-verify", "--no-gpg-sign", "-m", msg, NULL};
    const char *head[] = {"rev-parse", "HEAD", NULL};
    if (srt_git(fx->repo, add) != 0 || srt_git(fx->repo, commit) != 0 ||
        srt_git(fx->repo, head) != 0)
        return false;
    g_srt_out[strcspn(g_srt_out, "\r\n")] = '\0';
    snprintf(out, 64, "%s", g_srt_out);
    return strlen(out) == 40;
}

static bool srt_tree_p(const struct srt_fx *fx)
{
    char mk[PATH_MAX + 32];
    snprintf(mk, sizeof mk, "FX_TOOL := %s/fx-epoch-object\n", fx->tools);
    char *body = malloc(strlen(mk) + sizeof k_makefile_body);
    if (body == NULL)
        return false;
    memcpy(body, mk, strlen(mk));
    memcpy(body + strlen(mk), k_makefile_body, sizeof k_makefile_body);
    bool ok = srt_write(fx->repo, "Makefile", body) &&
              srt_write(fx->repo, ".gitignore", "build/\n") &&
              srt_write(fx->repo, "src/common.h", k_header_p) &&
              srt_write(fx->repo, "src/a.c", k_a_p) && srt_write(fx->repo, "src/b.c", k_b) &&
              srt_write(fx->repo, "src/c.c", k_c);
    free(body);
    return ok;
}

/* The repository: P, then C1 (a.c's body), then C2 (common.h's fx_a_value). */
static bool srt_history(struct srt_fx *fx)
{
    const char *init[] = {"-c", "init.defaultBranch=main", "init", "--quiet", NULL};
    return srt_mkdir_p(fx->repo) && srt_git(fx->repo, init) == 0 && srt_tree_p(fx) &&
           srt_commit(fx, "P: three TUs and a header", fx->p) &&
           srt_write(fx->repo, "src/a.c", k_a_c1) &&
           srt_commit(fx, "C1: change the body of a.c", fx->c1) &&
           srt_write(fx->repo, "src/common.h", k_header_c2) &&
           srt_commit(fx, "C2: change the inline function only a.c calls", fx->c2);
}

static bool srt_find_tool(struct srt_fx *fx)
{
    const char *bin = getenv("ZCL_SEM_REPLAY_BIN");
    if (bin == NULL || bin[0] == '\0')
        bin = "build/bin/z23-sem-replay";
    if (realpath(bin, fx->tool) == NULL || access(fx->tool, X_OK) != 0) {
        printf("(%s is a required prerequisite: make sem-replay-bin) ", bin);
        return false;
    }
    return true;
}

static bool srt_setup(struct srt_fx *fx)
{
    const char *probe[] = {"objcopy", "--version", NULL};
    test_make_tmpdir(fx->root, sizeof fx->root, "sem_replay", "fx");
    if (snprintf(fx->repo, sizeof fx->repo, "%s/repo", fx->root) >= (int)sizeof fx->repo ||
        snprintf(fx->tools, sizeof fx->tools, "%s/tools", fx->root) >= (int)sizeof fx->tools)
        return false;
    /* Without objcopy the replay cannot split code from debug changes and
     * files a changed object as unknown, which it treats as code. */
    fx->objcopy = srt_run(probe) == 0;
    return srt_find_tool(fx) && srt_mkdir_p(fx->tools) && srt_tools(fx) && srt_history(fx);
}

/* ── running the replay ──────────────────────────────────────────────── */

/* z23-sem-replay step for commit at index; state becomes <root>/state-<index>. */
static int srt_step(const struct srt_fx *fx, const char *planner, const char *commit,
                    const char *index, char state[PATH_MAX])
{
    char sensor[PATH_MAX + 16], plan[PATH_MAX + 64];
    snprintf(state, PATH_MAX, "%s/state-%s", fx->root, index);
    snprintf(sensor, sizeof sensor, "%s/fx-sensor", fx->tools);
    snprintf(plan, sizeof plan, "%s/%s", fx->tools, planner);
    const char *argv[] = {fx->tool, "step",   "--repo",  fx->repo, "--state",
                          state,    "--sensor", sensor,  "--planner", plan,
                          "--jobs", "2",      "--index", index,    "--commit",
                          commit,   NULL};
    return srt_run(argv);
}

/* The value of column name in a two-line result.tsv; "" when absent. */
static bool srt_result(const char *state, const char *dir, const char *name,
                       char *val, size_t cap)
{
    static char text[8192];
    char path[PATH_MAX + 64];
    snprintf(path, sizeof path, "%s/run/%s/result.tsv", state, dir);
    val[0] = '\0';
    if (!srt_read(path, text, sizeof text))
        return false;
    char *row = strchr(text, '\n');
    if (row == NULL)
        return false;
    *row++ = '\0';
    char *hs = NULL, *rs = NULL;
    char *h = strtok_r(text, "\t", &hs), *v = strtok_r(row, "\t\n", &rs);
    for (; h && v; h = strtok_r(NULL, "\t", &hs), v = strtok_r(NULL, "\t\n", &rs)) {
        if (strcmp(h, name) == 0) {
            snprintf(val, cap, "%s", v);
            return true;
        }
    }
    return false;
}

/* Does column name of the step's result.tsv equal want? */
static bool srt_col_is(const char *state, const char *dir, const char *name,
                       const char *want)
{
    char got[256];
    bool ok = srt_result(state, dir, name, got, sizeof got) && strcmp(got, want) == 0;
    if (!ok)
        printf("(result.tsv %s = \"%s\", want \"%s\") ", name, got, want);
    return ok;
}

static bool srt_file_has(const char *path, const char *needle)
{
    static char text[1 << 16];
    return srt_read(path, text, sizeof text) && strstr(text, needle) != NULL;
}

/* run/<NNN>_<first ten hex of commit> */
static void srt_run_dir(char out[32], int index, const char *commit)
{
    snprintf(out, 32, "%03d_%.10s", index, commit);
}

/* z23-sem-replay run over one commit, the tool named by its bare name on
 * PATH: run re-executes itself for the step, so this also proves it finds
 * its own path without /proc. */
static int srt_replay_run(const struct srt_fx *fx, const char *commit)
{
    static char pathvar[8192];
    char state[PATH_MAX + 16], commits[PATH_MAX + 16], sensor[PATH_MAX + 16];
    char plan[PATH_MAX + 32], bindir[PATH_MAX], text[80];
    const char *old = getenv("PATH");
    snprintf(bindir, sizeof bindir, "%s", fx->tool);
    *strrchr(bindir, '/') = '\0';
    snprintf(pathvar, sizeof pathvar, "PATH=%s:%s", bindir, old ? old : "/usr/bin:/bin");
    snprintf(state, sizeof state, "%s/state-run", fx->root);
    snprintf(commits, sizeof commits, "%s/commits.txt", fx->root);
    snprintf(sensor, sizeof sensor, "%s/fx-sensor", fx->tools);
    snprintf(plan, sizeof plan, "%s/fx-planner-omits", fx->tools);
    snprintf(text, sizeof text, "%s\n", commit);
    if (!srt_write(fx->root, "commits.txt", text))
        return -1;
    const char *argv[] = {pathvar,   strrchr(fx->tool, '/') + 1, "run", "--repo", fx->repo,
                          "--state", state, "--sensor", sensor, "--planner", plan,
                          "--jobs",  "2",   "--commits", commits, NULL};
    return srt_run(argv);
}

#endif /* !_WIN32 */

int test_sem_replay(void)
{
#if defined(_WIN32)
    printf("sem_replay: UNAVAILABLE on native Windows "
           "(the replay forks git, make and a compiler)\n");
    return 1;
#else
    int failures = 0;
    struct srt_fx fx = {0};
    char state_a[PATH_MAX] = "", state_b[PATH_MAX] = "", state_c[PATH_MAX] = "";
    char dir_a[32], dir_b[32], dir_c[32];
    char path[PATH_MAX + 64], want[512];

    TEST("the fixture history builds: P, C1 (a.c body), C2 (common.h)") {
        ASSERT(srt_setup(&fx));
        srt_run_dir(dir_a, 1, fx.c1);
        srt_run_dir(dir_b, 2, fx.c2);
        srt_run_dir(dir_c, 3, fx.c2);
        PASS();
    }

    TEST("a facts plan that leaves out the changed TU exits 3 (false negative)") {
        int rc = srt_step(&fx, "fx-planner-omits", fx.c1, "1", state_a);
        if (rc != SRT_EXIT_FALSE_NEGATIVE)
            printf("(step output: %s) ", g_srt_out);
        ASSERT_EQ(rc, SRT_EXIT_FALSE_NEGATIVE);
        snprintf(want, sizeof want, "FALSE NEGATIVE (code): commit %s TU src/a.c", fx.c1);
        ASSERT(strstr(g_srt_out, want) != NULL);
        PASS();
    }

    TEST("MISSES.tsv names src/a.c as the code false negative, exactly") {
        char text[4096];
        snprintf(path, sizeof path, "%s/MISSES.tsv", state_a);
        ASSERT(srt_read(path, text, sizeof text));
        snprintf(want, sizeof want,
                 "%s\tsrc/a.c\tFALSE NEGATIVE (code)\tprecise\tfixture-omits-the-changed-tu\n",
                 fx.c1);
        ASSERT_STR_EQ(text, want);
        snprintf(path, sizeof path, "%s/run/%s/sets.tsv", state_a, dir_a);
        ASSERT(srt_file_has(path, "\nfn_code\tsrc/a.c\n"));
        PASS();
    }

    TEST("run over the same commit re-executes itself and stops with exit 3") {
        int rc = srt_replay_run(&fx, fx.c1);
        if (rc != SRT_EXIT_FALSE_NEGATIVE)
            printf("(run output: %s) ", g_srt_out);
        ASSERT_EQ(rc, SRT_EXIT_FALSE_NEGATIVE);
        ASSERT(strstr(g_srt_out, "stopped at a code false negative") != NULL);
        PASS();
    }

    TEST("the omitting step counts make 1, changed 1, plain 1, facts 0, fn_code 1") {
        ASSERT(srt_col_is(state_a, dir_a, "kind", "body-only"));
        ASSERT(srt_col_is(state_a, dir_a, "make", "1"));
        ASSERT(srt_col_is(state_a, dir_a, "changed", "1"));
        ASSERT(srt_col_is(state_a, dir_a, "plain", "1"));
        ASSERT(srt_col_is(state_a, dir_a, "facts", "0"));
        ASSERT(srt_col_is(state_a, dir_a, "facts_mode", "precise"));
        ASSERT(srt_col_is(state_a, dir_a, "fn", "1"));
        ASSERT(srt_col_is(state_a, dir_a, "fn_code", "1"));
        ASSERT(srt_col_is(state_a, dir_a, "build_failed", "0"));
        PASS();
    }

    TEST("a facts plan that narrows to the changed TU exits 0") {
        int rc = srt_step(&fx, "fx-planner-narrows", fx.c2, "2", state_b);
        if (rc != 0)
            printf("(step output: %s) ", g_srt_out);
        ASSERT_EQ(rc, 0);
        PASS();
    }

    TEST("both sides' manifests name the object compiler and the toolchain identity, "
        "as make clang-facts senses them") {
        const char *sides[] = {"before", "after"};
        for (size_t s = 0; s < 2; s++) {
            snprintf(path, sizeof path, "%s/build/sem-replay/facts/src/a.c.%s.zsm", fx.repo,
                     sides[s]);
            if (!srt_file_has(path, "cc=cc toolchain=" SRT_TOOLCHAIN "\n"))
                printf("(%s manifest lacks --cc cc --toolchain-id) ", sides[s]);
            ASSERT(srt_file_has(path, "cc=cc toolchain=" SRT_TOOLCHAIN "\n"));
        }
        PASS();
    }

    TEST("the narrowing step's compile counts are exact: make 2, changed 1, plain 2, facts 1") {
        ASSERT(srt_col_is(state_b, dir_b, "kind", "header"));
        ASSERT(srt_col_is(state_b, dir_b, "files", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "h", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "make", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "changed", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "code_changed", fx.objcopy ? "1" : "0"));
        ASSERT(srt_col_is(state_b, dir_b, "unknown_changed", fx.objcopy ? "0" : "1"));
        ASSERT(srt_col_is(state_b, dir_b, "debug_changed", "0"));
        ASSERT(srt_col_is(state_b, dir_b, "plain", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "facts", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "facts_mode", "precise"));
        ASSERT(srt_col_is(state_b, dir_b, "fn", "0"));
        ASSERT(srt_col_is(state_b, dir_b, "plain_fn", "0"));
        ASSERT(srt_col_is(state_b, dir_b, "drift", "0"));
        ASSERT(srt_col_is(state_b, dir_b, "cold_checked", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "cold_mismatch", "0"));
        ASSERT(srt_col_is(state_b, dir_b, "build_failed", "0"));
        PASS();
    }

    TEST("the narrowing step's group counts are exact: plain 2, facts 1") {
        ASSERT(srt_col_is(state_b, dir_b, "groups_plain", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "groups_facts", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "obl_plain", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "obl_facts", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "narrowed", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "uni_complete", "1"));
        ASSERT(srt_col_is(state_b, dir_b, "uni_total", "2"));
        ASSERT(srt_col_is(state_b, dir_b, "uni_affected", "1"));
        PASS();
    }

    TEST("the narrowing step's sets name the exact TUs") {
        snprintf(path, sizeof path, "%s/run/%s/sets.tsv", state_b, dir_b);
        ASSERT(srt_file_has(path, "\nmake\tsrc/a.c\nmake\tsrc/b.c\nchanged\tsrc/a.c\nplain\t"));
        ASSERT(srt_file_has(path, "\nplain\tsrc/a.c\nplain\tsrc/b.c\nfacts\tsrc/a.c\ntu\tsrc/a.c\t"));
        ASSERT(!srt_file_has(path, "\nfn_code\t"));
        snprintf(path, sizeof path, "%s/MISSES.tsv", state_b);
        ASSERT(access(path, F_OK) != 0);
        PASS();
    }

    TEST("the report headline counts 1 compile and 1 test group avoided, "
        "false-narrow 0, false-wide facts 0, false-wide plain 1") {
        const char *argv[] = {fx.tool, "report", "--state", state_b, NULL};
        ASSERT_EQ(srt_run(argv), 0);
        ASSERT(strstr(g_srt_out, "Compiler executions avoided vs make: 1 (make compiled 2 "
                                 "TUs, the facts plan 1).\n") != NULL);
        ASSERT(strstr(g_srt_out, "Test-group executions avoided vs plain: 1 (plain "
                                 "selected 2 groups, the facts plan 1).\n") != NULL);
        ASSERT(strstr(g_srt_out, "False-narrow (a changed object the facts plan left out; "
                                 "must be 0, fails the run): 0.\n") != NULL);
        ASSERT(strstr(g_srt_out, "False-wide vs facts (a TU the facts plan compiled whose "
                                 "object bytes did not change): 0.\n") != NULL);
        ASSERT(strstr(g_srt_out, "False-wide vs plain (a TU the plain plan compiled whose "
                                 "object bytes did not change): 1.\n") != NULL);
        PASS();
    }

    TEST("the report puts catalog groups and plan tokens in separate columns, each "
        "plain / facts") {
        const char *argv[] = {fx.tool, "report", "--state", state_b, NULL};
        ASSERT_EQ(srt_run(argv), 0);
        ASSERT(strstr(g_srt_out, "| groups plain / facts | tokens plain / facts |") != NULL);
        ASSERT(strstr(g_srt_out, "| 0/0/0 | 2 / 1 | 2 / 1 | narrowed |") != NULL);
        PASS();
    }

    TEST("a facts plan that over-selects the unaffected TU exits 0 (not a false negative)") {
        int rc = srt_step(&fx, "fx-planner-overselects", fx.c2, "3", state_c);
        if (rc != 0)
            printf("(step output: %s) ", g_srt_out);
        ASSERT_EQ(rc, 0);
        PASS();
    }

    TEST("the over-selecting step's counts are exact: false-narrow 0, false-wide facts 1, "
        "false-wide plain 1") {
        ASSERT(srt_col_is(state_c, dir_c, "fn_code", "0"));
        ASSERT(srt_col_is(state_c, dir_c, "fw_facts", "1"));
        ASSERT(srt_col_is(state_c, dir_c, "fw_plain", "1"));
        snprintf(path, sizeof path, "%s/run/%s/sets.tsv", state_c, dir_c);
        ASSERT(srt_file_has(path, "\nfw_facts\tsrc/b.c\n"));
        ASSERT(srt_file_has(path, "\nfw_plain\tsrc/b.c\n"));
        PASS();
    }

_test_next:;
    if (fx.root[0])
        (void)test_rm_rf_recursive(fx.root);
    if (failures == 0)
        printf("test_sem_replay: all passed\n");
    else
        printf("test_sem_replay: %d FAILED\n", failures);
    return failures;
#endif
}
