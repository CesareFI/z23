/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#define _POSIX_C_SOURCE 200809L
#include "blue_chain_tip.h"
#include "json/json.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const struct json_value *unique_field(const struct json_value *object,
                                             const char *name) {
    if (!object || object->type != JSON_OBJ) return NULL;
    const struct json_value *found = NULL;
    for (size_t i = 0; i < object->num_children; ++i) {
        if (strcmp(object->keys[i], name) != 0) continue;
        if (found) return NULL;
        found = &object->children[i];
    }
    return found;
}

static bool hex_hash(const char *text) {
    if (!text || strlen(text) != 64) return false;
    for (size_t i = 0; i < 64; ++i) {
        char digit = text[i];
        if ((digit < '0' || digit > '9') &&
            (digit < 'a' || digit > 'f') &&
            (digit < 'A' || digit > 'F')) return false;
    }
    return true;
}

static bool mainnet_identity(const struct json_value *result) {
    const struct json_value *chain = unique_field(result, "chain");
    const struct json_value *hash = unique_field(result, "bestblockhash");
    const struct json_value *ibd = unique_field(result, "initialblockdownload");
    return chain && chain->type == JSON_STR &&
        strcmp(chain->val.s, "main") == 0 && hash &&
        hash->type == JSON_STR && hex_hash(hash->val.s) &&
        (!ibd || (ibd->type == JSON_BOOL && !ibd->val.b));
}

static bool synced_next_height(const struct json_value *result,
                               uint32_t *next_height) {
    const struct json_value *blocks = unique_field(result, "blocks");
    const struct json_value *headers = unique_field(result, "headers");
    if (!blocks || blocks->type != JSON_INT || blocks->val.i < 0 ||
        blocks->val.i >= INT_MAX || !headers || headers->type != JSON_INT ||
        headers->val.i != blocks->val.i) return false;
    *next_height = (uint32_t)(blocks->val.i + 1);
    return true;
}

bool blue_chain_tip_parse(const char *reply, size_t length,
                          uint32_t *next_height) {
    if (!reply || !next_height || !length || length > 65535) return false;
    struct json_value root = {0};
    bool valid = json_read(&root, reply, length);
    const struct json_value *result = valid ? unique_field(&root, "result") : NULL;
    const struct json_value *error = valid ? unique_field(&root, "error") : NULL;
    valid = result && result->type == JSON_OBJ && error &&
        error->type == JSON_NULL && mainnet_identity(result) &&
        synced_next_height(result, next_height);
    json_free(&root);
    return valid;
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool read_reply(int fd, char *reply, size_t capacity, size_t *used) {
    int64_t deadline = monotonic_ms() + 7000;
    *used = 0;
    while (*used < capacity) {
        int64_t remaining = deadline - monotonic_ms();
        if (remaining <= 0 || remaining > INT_MAX) return false;
        struct pollfd entry = {.fd = fd, .events = POLLIN};
        int ready = poll(&entry, 1, (int)remaining);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return false;
        ssize_t count = read(fd, reply + *used, capacity - *used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) return false;
        if (count == 0) return true;
        *used += (size_t)count;
    }
    return false;
}

static bool finish_child(pid_t child, bool complete) {
    if (!complete) kill(child, SIGKILL);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); }
    while (waited < 0 && errno == EINTR);
    return complete && waited == child && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0;
}

bool blue_chain_tip_query(const char *rpc_binary, uint32_t *next_height) {
    if (!rpc_binary || rpc_binary[0] != '/' || !next_height) return false;
    int pipefd[2];
    if (pipe(pipefd) != 0) return false;
    pid_t child = fork();
    if (child < 0) {
        close(pipefd[0]); close(pipefd[1]);
        return false;
    }
    if (child == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0 ||
            setenv("ZCL_RPC_MAX_TIME_SECS", "5", 1) != 0)
            _exit(127);
        close(pipefd[1]);
        execl(rpc_binary, rpc_binary, "getblockchaininfo", (char *)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    char reply[65536];
    size_t used = 0;
    bool complete = read_reply(pipefd[0], reply, sizeof reply, &used);
    close(pipefd[0]);
    return finish_child(child, complete) &&
        blue_chain_tip_parse(reply, used, next_height);
}
