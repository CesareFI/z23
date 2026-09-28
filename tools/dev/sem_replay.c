/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * z23-sem-replay: measure, on real commits, what the semantic facts plan
 * saves in compiles and test groups, and check it against the objects make
 * actually rebuilt. See docs/work/SEMANTIC_MANIFEST.md, "Replay on real
 * history".
 *
 *   z23-sem-replay run    --repo R --state S --sensor X --planner Y
 *                         --commits FILE [--jobs N] [--devbuild PATH]
 *   z23-sem-replay step   ... --index I --commit C
 *   z23-sem-replay repro  --repo R --state S [--label L] [--jobs N]
 *   z23-sem-replay report --state S
 *
 * run replays each commit of FILE (one full SHA per line, oldest first) in
 * its own step, through `devbuild --wait` when --devbuild names it, so the
 * host scheduler admits each commit's builds and parses separately. A
 * finished step leaves run/<NN>_<commit>/result.tsv and is skipped when the
 * run resumes. A false negative stops the run (exit 3). R is a dedicated
 * worktree the replay checks out; it must hold no other work. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sem_replay.h"
#include "sem_replay_step.h"

static int usage(void)
{
    fprintf(stderr,
            "usage: z23-sem-replay run|step|repro|report --repo DIR --state DIR\n"
            "       [--sensor BIN] [--planner BIN] [--commits FILE] [--jobs N]\n"
            "       [--devbuild BIN] [--index N] [--commit SHA] [--label NAME]\n");
    return 2;
}

struct opts {
    struct sr_cfg cfg;
    char commits[SR_PATH];
    char label[64];
};

static bool abs_path(const char *in, char out[SR_PATH])
{
    char buf[PATH_MAX];
    if (realpath(in, buf) == NULL) {
        fprintf(stderr, "sem-replay: %s: %s\n", in, strerror(errno));
        return false;
    }
    snprintf(out, SR_PATH, "%s", buf);
    return true;
}

static bool opt_value(struct opts *o, const char *key, const char *v)
{
    struct sr_cfg *c = &o->cfg;
    if (strcmp(key, "--repo") == 0)
        return abs_path(v, c->repo);
    if (strcmp(key, "--state") == 0)
        return sr_mkdirs(v) && abs_path(v, c->state);
    if (strcmp(key, "--sensor") == 0)
        return abs_path(v, c->sensor);
    if (strcmp(key, "--planner") == 0)
        return abs_path(v, c->planner);
    if (strcmp(key, "--devbuild") == 0)
        return abs_path(v, c->devbuild);
    if (strcmp(key, "--commits") == 0)
        return abs_path(v, o->commits);
    if (strcmp(key, "--jobs") == 0)
        return (c->jobs = atoi(v)) > 0 && c->jobs <= 8;
    if (strcmp(key, "--index") == 0)
        return (c->index = atoi(v)) >= 0;
    if (strcmp(key, "--commit") == 0)
        return snprintf(c->commit, sizeof(c->commit), "%s", v) > 0;
    if (strcmp(key, "--label") == 0)
        return snprintf(o->label, sizeof(o->label), "%s", v) > 0;
    fprintf(stderr, "sem-replay: unknown option %s\n", key);
    return false;
}

static bool parse_opts(int argc, char **argv, struct opts *o)
{
    memset(o, 0, sizeof(*o));
    o->cfg.jobs = 8;
    snprintf(o->label, sizeof(o->label), "check");
    if (!abs_path("/proc/self/exe", o->cfg.self))
        return false;
    for (int i = 2; i + 1 < argc; i += 2)
        if (!opt_value(o, argv[i], argv[i + 1]))
            return false;
    return argc % 2 == 0 && o->cfg.state[0] != '\0';
}

static bool step_done(const struct sr_cfg *c, int index, const char *commit)
{
    char path[SR_PATH];
    snprintf(path, sizeof(path), "%s/run/%03d_%.10s/result.tsv", c->state, index, commit);
    return sr_exists(path);
}

static int spawn_step(const struct sr_cfg *c, int index, const char *commit)
{
    char idx[16], jobs[16], log[SR_PATH];
    snprintf(idx, sizeof(idx), "%d", index);
    snprintf(jobs, sizeof(jobs), "%d", c->jobs);
    snprintf(log, sizeof(log), "%s/run.log", c->state);
    char *tail[] = {(char *)c->self, "step", "--repo", (char *)c->repo, "--state",
                    (char *)c->state, "--sensor", (char *)c->sensor, "--planner",
                    (char *)c->planner, "--jobs", jobs, "--index", idx, "--commit",
                    (char *)commit, NULL};
    char *with[20] = {(char *)c->devbuild, "--wait"};
    size_t k = 2;
    for (size_t i = 0; tail[i]; i++)
        with[k++] = tail[i];
    with[k] = NULL;
    return sr_run(c->devbuild[0] ? with : tail, c->repo, log, NULL, NULL);
}

static int run_all(const struct opts *o)
{
    char *text = NULL;
    size_t len = 0;
    if (!sr_read_file(o->commits, &text, &len)) {
        fprintf(stderr, "sem-replay: cannot read %s\n", o->commits);
        return 2;
    }
    int rc = 0, index = 0;
    for (char *line = strtok(text, "\n"); line && rc == 0; line = strtok(NULL, "\n")) {
        index++;
        line[strcspn(line, " \t\r")] = '\0';
        if (line[0] == '\0' || line[0] == '#' || step_done(&o->cfg, index, line))
            continue;
        fprintf(stderr, "sem-replay: step %d %s\n", index, line);
        rc = spawn_step(&o->cfg, index, line);
    }
    free(text);
    if (rc == SR_STEP_FALSE_NEGATIVE)
        fprintf(stderr, "sem-replay: stopped at a FALSE NEGATIVE (see FALSE_NEGATIVES.tsv)\n");
    return rc;
}

static bool need_tools(const struct sr_cfg *c)
{
    bool ok = c->repo[0] && c->sensor[0] && c->planner[0];
    if (!ok)
        fprintf(stderr, "sem-replay: --repo, --sensor and --planner are required\n");
    return ok;
}

int main(int argc, char **argv)
{
    struct opts o;
    if (argc < 2 || !parse_opts(argc, argv, &o))
        return usage();
    const char *cmd = argv[1];
    if (strcmp(cmd, "report") == 0)
        return sr_report(&o.cfg);
    if (strcmp(cmd, "repro") == 0)
        return o.cfg.repo[0] ? sr_repro(&o.cfg, o.label) : usage();
    if (!need_tools(&o.cfg))
        return usage();
    if (strcmp(cmd, "run") == 0)
        return o.commits[0] ? run_all(&o) : usage();
    if (strcmp(cmd, "step") == 0)
        return o.cfg.commit[0] ? sr_step(&o.cfg) : usage();
    return usage();
}
