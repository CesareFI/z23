/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * Request-size guards for the loopback JSON-RPC client. The client composes
 * its POST body in a fixed stack buffer with snprintf; a request whose
 * composed length exceeds that buffer must be refused before connecting —
 * never sent with the would-have-written length, which would read past the
 * buffer and publish a Content-Length the truncated body cannot satisfy.
 * These scenarios prove the guard at the wire with a real loopback listener
 * in a forked child, so no live node is involved.
 *
 * Sibling to test_rpc.c (the group entry point). Fixtures here are private
 * to these scenarios. */

#include "test/test_core.h"
#include "controllers/rpc_client.h"
#include "base/safe_alloc.h"
#include "platform/socket_compat.h"
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define RR_REQUEST_CAP (256u * 1024u)
#define RR_ACCEPT_MS 2000
#define RR_RECV_IDLE_S 2

static const char rr_reply_body[] = "{\"result\":\"pong\",\"error\":null,\"id\":1}";

/* Build the listener socket on an ephemeral loopback port. */
static int rr_listen(uint16_t *port_out)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }
    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0) {
        close(fd);
        return -1;
    }
    *port_out = ntohs(addr.sin_port);
    return fd;
}

/* Locate seq within buf[0,len). Returns true and sets *at on success. */
static bool rr_find_seq(const char *buf, size_t len, const char *seq,
                        size_t seq_len, size_t *at)
{
    if (seq_len == 0 || len < seq_len)
        return false;
    for (size_t i = 0; i + seq_len <= len; i++) {
        if (memcmp(buf + i, seq, seq_len) == 0) {
            *at = i;
            return true;
        }
    }
    return false;
}

/* True once the buffered bytes hold a complete HTTP request: header
 * terminator found and the body bytes satisfy the declared
 * Content-Length. */
static bool rr_request_complete(const char *buf, size_t len)
{
    size_t hdr_at = 0;
    if (!rr_find_seq(buf, len, "\r\n\r\n", 4, &hdr_at))
        return false;
    size_t cl_at = 0;
    if (!rr_find_seq(buf, hdr_at, "Content-Length:", 15, &cl_at))
        return false;
    const char *value = buf + cl_at + 15;
    const char *line_end = memchr(value, '\r', hdr_at - (cl_at + 15));
    if (!line_end)
        return false;
    char tmp[32];
    size_t vlen = (size_t)(line_end - value);
    if (vlen == 0 || vlen >= sizeof(tmp))
        return false;
    memcpy(tmp, value, vlen);
    tmp[vlen] = 0;
    errno = 0;
    char *end = NULL;
    unsigned long long content_length = strtoull(tmp, &end, 10);
    if (errno != 0 || !end || end == tmp || *end != 0)
        return false;
    size_t body_len = len - (hdr_at + 4);
    return body_len >= content_length;
}

/* Child-side: serve exactly one connection. Exits 0 when a complete,
 * well-formed request arrived (replying with the canned JSON-RPC body when
 * one is provided), 1 when the connection arrived but the request was
 * truncated or malformed, 2 when no connection arrived before the accept
 * deadline, 3 on internal error. */

/* Accept one connection within RR_ACCEPT_MS; _exit(2)/(3) on refusal. */
static int rr_accept_one(int listen_fd)
{
    struct pollfd pfd = { .fd = listen_fd, .events = POLLIN, .revents = 0 };
    int pr;
    do {
        pr = poll(&pfd, 1, RR_ACCEPT_MS);
    } while (pr < 0 && errno == EINTR);
    if (pr <= 0)
        _exit(2);
    int c = accept(listen_fd, NULL, NULL);
    if (c < 0)
        _exit(3);
    return c;
}

/* Read until the request completes or the peer goes away; true when a
 * complete request sits in buf. */
static bool rr_recv_request(int c, char *buf, size_t cap, size_t *len_out)
{
    size_t len = 0;
    bool complete = false;
    for (;;) {
        ssize_t n = recv(c, buf + len, cap - len, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (n == 0)
            break;
        len += (size_t)n;
        if (len >= cap || rr_request_complete(buf, len)) {
            complete = true;
            break;
        }
    }
    *len_out = len;
    return complete;
}

/* Best-effort canned reply; the client's verdict never depends on it. */
static void rr_send_reply(int c, const char *reply_body)
{
    char reply[512];
    int rlen = snprintf(reply, sizeof(reply),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        strlen(reply_body), reply_body);
    size_t reply_len = rlen > 0 ? (size_t)rlen : 0;
    size_t sent = 0;
    while (sent < reply_len) {
        ssize_t n = send(c, reply + sent, reply_len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        sent += (size_t)n;
    }
}

static void rr_serve_child(int listen_fd, const char *reply_body)
{
    int c = rr_accept_one(listen_fd);
    struct timeval idle = { .tv_sec = RR_RECV_IDLE_S, .tv_usec = 0 };
    (void)setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &idle, sizeof(idle));
    static char buf[RR_REQUEST_CAP];
    size_t len = 0;
    bool complete = rr_recv_request(c, buf, sizeof(buf), &len);
    if (complete && reply_body)
        rr_send_reply(c, reply_body);
    close(c);
    close(listen_fd);
    _exit(complete ? 0 : 1);
}

/* Compose ["aaaa...a"] with exactly inner 'a' characters. */
static char *rr_params_of_size(size_t inner)
{
    char *p = zcl_malloc(inner + 5, "rr params");
    if (!p)
        return NULL;
    p[0] = '[';
    p[1] = '"';
    memset(p + 2, 'a', inner);
    p[2 + inner] = '"';
    p[3 + inner] = ']';
    p[4 + inner] = 0;
    return p;
}

/* Fork the listener child, run one client call, and reap the verdict. */
static char *rr_call_with_listener(const char *datadir, uint16_t port,
                                   const char *params, const char *reply_body,
                                   int *child_verdict)
{
    int listen_fd = rr_listen(&port);
    if (listen_fd < 0)
        return NULL;
    pid_t pid = fork();
    if (pid == 0)
        rr_serve_child(listen_fd, reply_body);
    char *resp = node_rpc_call_at_deadline(datadir, (int)port, "rrmethod",
                                           params, 250, 5000);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
    close(listen_fd);
    if (!WIFEXITED(status)) {
        *child_verdict = 3;
        return resp;
    }
    *child_verdict = WEXITSTATUS(status);
    return resp;
}

static int rr_write_cookie(const char *dir)
{
    char path[1024];
    int n = snprintf(path, sizeof(path), "%s/.cookie", dir);
    if (n <= 0 || (size_t)n >= sizeof(path))
        return -1;
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs("rruser:rrpass\n", f);
    return fclose(f) == 0 ? 0 : -1;
}

int check_rpc_node_client_rejects_oversized_request(void)
{
    int failures = 0;

    printf("rpc_node_client_rejects_oversized_request... ");
    {
        char dir[1024];
        test_make_tmpdir(dir, sizeof(dir), "rpc", "reqsize-big");
        node_rpc_client_set_test_hook(NULL);
        bool ok = rr_write_cookie(dir) == 0;
        int verdict = 3;
        char *resp = NULL;
        char *params = rr_params_of_size(9000);
        if (!params)
            ok = false;
        if (ok)
            resp = rr_call_with_listener(dir, 0, params, NULL, &verdict);
        /* The guard must fire before connecting: the client names the
         * refusal and the listener sees no connection at all. Before the
         * guard, the client connected, sent the 8192-byte truncated buffer
         * under a ~9 KB Content-Length, and either hung until the deadline
         * or returned an unrelated transport error. */
        ok = ok && resp && strstr(resp, "too large") != NULL &&
             verdict == 2;
        if (ok) {
            printf("OK\n");
        } else {
            printf("FAIL (verdict=%d refusal=%s)\n", verdict,
                   resp && strstr(resp, "too large") ? "named" : "missing");
            failures++;
        }
        free(resp);
        free(params);
        test_rm_rf(dir);
    }

    return failures;
}

int check_rpc_node_client_sends_max_sized_request(void)
{
    int failures = 0;

    printf("rpc_node_client_sends_max_sized_request... ");
    {
        char dir[1024];
        test_make_tmpdir(dir, sizeof(dir), "rpc", "reqsize-max");
        node_rpc_client_set_test_hook(NULL);
        bool ok = rr_write_cookie(dir) == 0;
        int verdict = 3;
        char *resp = NULL;
        char *params = rr_params_of_size(6000);
        if (!params)
            ok = false;
        if (ok)
            resp = rr_call_with_listener(dir, 0, params, rr_reply_body,
                                         &verdict);
        /* A request comfortably inside the buffer must round-trip intact:
         * the listener saw every byte the Content-Length promised, and the
         * client parsed the result out of the reply. */
        ok = ok && resp && strstr(resp, "pong") != NULL && verdict == 0;
        if (ok) {
            printf("OK\n");
        } else {
            printf("FAIL (verdict=%d)\n", verdict);
            failures++;
        }
        free(resp);
        free(params);
        test_rm_rf(dir);
    }

    return failures;
}
