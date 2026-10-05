/* zdeque CLI: exercise the deque over lines from stdin.
 *
 *   zdeque reverse    print stdin lines in reverse order (LIFO)
 *   zdeque rotate N   rotate the line queue left by N positions
 *
 * Demonstrates a real consumer of the two-ended container.
 */
#include "zdeque/zdeque.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 4096
#define MAX_LINE 512

static int parse_rotation(int argc, char **argv, long *rotation)
{
    if (argc < 3) return 0;
    char *end;
    errno = 0;
    const long value = strtol(argv[2], &end, 10);
    if (errno == ERANGE || end == argv[2] || *end != '\0') return 0;
    *rotation = value;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2 || (strcmp(argv[1], "reverse") != 0 &&
                     strcmp(argv[1], "rotate") != 0)) {
        fprintf(stderr, "usage: zdeque <reverse|rotate N>\n");
        return 2;
    }

    static char lines[MAX_LINES][MAX_LINE];
    static void *slots[MAX_LINES];
    zdeque dq;
    if (zdeque_init(&dq, slots, MAX_LINES) != ZDEQUE_OK) return 1;

    size_t n = 0;
    while (n < MAX_LINES && fgets(lines[n], MAX_LINE, stdin)) {
        size_t l = strlen(lines[n]);
        if (l > 0 && lines[n][l - 1] == '\n') lines[n][l - 1] = '\0';
        if (zdeque_push_back(&dq, lines[n]) != ZDEQUE_OK) break;
        n++;
    }

    if (strcmp(argv[1], "reverse") == 0) {
        void *p;
        while (zdeque_pop_back(&dq, &p) == ZDEQUE_OK)
            printf("%s\n", (char *)p);
        return 0;
    }

    /* rotate N */
    long rot;
    if (!parse_rotation(argc, argv, &rot)) {
        fprintf(stderr, "usage: zdeque rotate N\n");
        return 2;
    }
    size_t sz = zdeque_size(&dq);
    if (sz == 0) return 0;
    long r = ((rot % (long)sz) + (long)sz) % (long)sz;
    for (long i = 0; i < r; i++) {
        void *p = NULL;
        zdeque_pop_front(&dq, &p);
        zdeque_push_back(&dq, p);
    }
    void *p;
    while (zdeque_pop_front(&dq, &p) == ZDEQUE_OK)
        printf("%s\n", (char *)p);
    return 0;
}
