/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: zlru test suite.  Exits nonzero on the first failure. */
#include "zlru/zlru.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                            \
    }                                                                        \
  } while (0)

/* Values are (void*)(intptr_t)n in these tests; a destructor records
 * the values it saw. */
static int destroyed[64];
static size_t destroyed_len; /* recorded (capped) for destroyed_has */
static size_t destroyed_calls; /* exact total destructor calls */

static void record_destroy(void *ctx, const char *key, void *value) {
  (void)ctx;
  (void)key;
  destroyed_calls++;
  if (destroyed_len < sizeof(destroyed) / sizeof(destroyed[0]))
    destroyed[destroyed_len++] = (int)(intptr_t)value;
}

static bool destroyed_has(int v) {
  for (size_t i = 0; i < destroyed_len; i++)
    if (destroyed[i] == v)
      return true;
  return false;
}

#define VP(n) ((void *)(intptr_t)(n))

static void test_basic(void) {
  destroyed_len = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(4, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c != NULL);
  CHECK(zlru_capacity(c) == 4);
  CHECK(zlru_size(c) == 0);
  CHECK(zlru_get(c, "missing") == NULL);
  CHECK(zlru_put(c, "a", VP(1)));
  CHECK(zlru_put(c, "b", VP(2)));
  CHECK(zlru_size(c) == 2);
  CHECK(zlru_get(c, "a") == VP(1));
  CHECK(zlru_get(c, "b") == VP(2));
  CHECK(destroyed_len == 0);
  zlru_destroy(c);
  CHECK(destroyed_has(1) && destroyed_has(2));
  CHECK(destroyed_len == 2);
}

static void test_eviction_order(void) {
  destroyed_len = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(3, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c);
  zlru_put(c, "a", VP(1)); /* LRU -> MRU: a */
  zlru_put(c, "b", VP(2)); /* a b */
  zlru_put(c, "c", VP(3)); /* a b c */
  zlru_get(c, "a");        /* b c a  (a promoted) */
  zlru_put(c, "d", VP(4)); /* evicts b: c a d */
  CHECK(zlru_size(c) == 3);
  CHECK(zlru_get(c, "b") == NULL);
  CHECK(destroyed_len == 1 && destroyed[0] == 2);
  zlru_put(c, "e", VP(5)); /* evicts c: a d e */
  CHECK(destroyed_has(3));
  CHECK(zlru_get(c, "a") == VP(1));
  CHECK(zlru_get(c, "d") == VP(4));
  CHECK(zlru_get(c, "e") == VP(5));
  zlru_destroy(c);
}

static void test_replace(void) {
  destroyed_len = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(2, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c);
  zlru_put(c, "k", VP(1));
  zlru_put(c, "k", VP(2)); /* replace: destructor on 1 */
  CHECK(zlru_size(c) == 1);
  CHECK(zlru_get(c, "k") == VP(2));
  CHECK(destroyed_len == 1 && destroyed[0] == 1);
  /* Replace promotes: put x, re-put k (now MRU), then y evicts x. */
  zlru_put(c, "x", VP(3)); /* k x */
  zlru_put(c, "k", VP(2)); /* replace again: x k */
  CHECK(destroyed_calls == 2);
  zlru_put(c, "y", VP(4)); /* evicts x (LRU), keeps k */
  CHECK(zlru_get(c, "x") == NULL);
  CHECK(zlru_get(c, "k") == VP(2));
  zlru_destroy(c);
}

static void test_erase(void) {
  destroyed_len = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(4, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c);
  zlru_put(c, "a", VP(1));
  zlru_put(c, "b", VP(2));
  zlru_put(c, "c", VP(3));
  zlru_erase(c, "b"); /* middle */
  CHECK(destroyed_len == 1 && destroyed[0] == 2);
  zlru_erase(c, "absent"); /* no-op */
  CHECK(zlru_size(c) == 2);
  zlru_erase(c, "a");
  zlru_erase(c, "c");
  CHECK(zlru_size(c) == 0);
  /* Cache still works after being emptied. */
  zlru_put(c, "z", VP(9));
  CHECK(zlru_get(c, "z") == VP(9));
  zlru_destroy(c);
}

static void test_capacity_one(void) {
  destroyed_len = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(1, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c);
  zlru_put(c, "a", VP(1));
  zlru_put(c, "b", VP(2)); /* evicts a immediately */
  CHECK(zlru_size(c) == 1);
  CHECK(zlru_get(c, "a") == NULL);
  CHECK(zlru_get(c, "b") == VP(2));
  CHECK(destroyed_len == 1 && destroyed[0] == 1);
  zlru_destroy(c);
}

static bool collect_key(void *ctx, const char *key, void *value) {
  (void)value;
  strcat(ctx, key);
  strcat(ctx, " ");
  return true;
}

static void test_visit_order(void) {
  zlru *c = zlru_create(8, NULL, NULL, (zmap_alloc){0});
  CHECK(c);
  zlru_put(c, "a", VP(1));
  zlru_put(c, "b", VP(2));
  zlru_put(c, "c", VP(3));
  zlru_get(c, "a"); /* promote a: order b? no: c? order is b? recompute */
  /* MRU order now: a c b */
  char acc[64] = "";
  zlru_visit_mru_first(c, collect_key, acc);
  CHECK(!strcmp(acc, "a c b "));
  /* The visit must not promote: same order again. */
  memset(acc, 0, sizeof(acc));
  zlru_visit_mru_first(c, collect_key, acc);
  CHECK(!strcmp(acc, "a c b "));
  zlru_destroy(c);
}

static bool stop_after_two(void *ctx, const char *key, void *value) {
  static int seen;
  (void)key;
  (void)value;
  (*(int *)ctx)++;
  seen = *(int *)ctx;
  return seen % 3 != 0 ? true : false;
}

static void test_visit_early_stop(void) {
  zlru *c = zlru_create(8, NULL, NULL, (zmap_alloc){0});
  zlru_put(c, "a", VP(1));
  zlru_put(c, "b", VP(2));
  zlru_put(c, "c", VP(3));
  int seen = 0;
  zlru_visit_mru_first(c, stop_after_two, &seen);
  CHECK(seen >= 1);
  zlru_destroy(c);
}

/* Allocator that fails after a fixed number of allocations. */
typedef struct {
  size_t remaining;
  size_t live;
} fail_ctx;

static void *fail_alloc(void *ctx, size_t size) {
  fail_ctx *f = ctx;
  if (!f->remaining)
    return NULL;
  f->remaining--;
  void *p = calloc(1, size);
  if (p)
    f->live++;
  return p;
}

static void fail_dealloc(void *ctx, void *ptr) {
  fail_ctx *f = ctx;
  if (ptr) {
    CHECK(f->live > 0);
    f->live--;
  }
  free(ptr);
}

static void test_alloc_failure(void) {
  /* create fails when the map allocation is denied. */
  {
    fail_ctx f = {.remaining = 1}; /* cache struct only */
    zmap_alloc a = {&f, fail_alloc, fail_dealloc};
    zlru *c = zlru_create(4, NULL, NULL, a);
    CHECK(c == NULL);
    CHECK(f.live == 0);
  }
  /* put fails cleanly when the node allocation is denied. */
  {
    fail_ctx f = {.remaining = 64};
    zmap_alloc a = {&f, fail_alloc, fail_dealloc};
    zlru *c = zlru_create(4, record_destroy, NULL, a);
    CHECK(c);
    zlru_put(c, "a", VP(1));
    f.remaining = 0;
    destroyed_len = 0;
    destroyed_calls = 0;
    CHECK(!zlru_put(c, "b", VP(2)));
    CHECK(zlru_size(c) == 1); /* unchanged */
    CHECK(zlru_get(c, "a") == VP(1)); /* still intact */
    CHECK(destroyed_len == 0);
    zlru_destroy(c);
    CHECK(f.live == 0);
  }
  /* create with capacity 0 is rejected. */
  CHECK(zlru_create(0, NULL, NULL, (zmap_alloc){0}) == NULL);
}

static void check_owned_key(void *ctx, const char *key, void *value) {
  CHECK(strcmp(key, ctx) == 0);
  CHECK(value == VP(7));
  destroyed_calls++;
}

static bool visit_owned_key(void *ctx, const char *key, void *value) {
  CHECK(strcmp(key, ctx) == 0);
  CHECK(value == VP(7));
  return true;
}

static void test_entry_key_lifetime(void) {
  fail_ctx f = {.remaining = 64};
  char expected[] = "copied";
  char source[] = "copied";
  zmap_alloc a = {&f, fail_alloc, fail_dealloc};
  zlru *c = zlru_create(2, check_owned_key, expected, a);
  CHECK(c != NULL);
  if (!c) return;
  size_t before = f.live;
  f.remaining = 2; /* One node/key allocation plus the independent map key. */
  destroyed_calls = 0;
  CHECK(zlru_put(c, source, VP(7)));
  CHECK(f.remaining == 0 && f.live == before + 2);
  memset(source, 'x', sizeof(source) - 1);
  CHECK(zlru_get(c, expected) == VP(7));
  zlru_visit_mru_first(c, visit_owned_key, expected);
  zlru_erase(c, expected); /* Callback's key survives the map key's free. */
  CHECK(destroyed_calls == 1 && f.live == before);
  zlru_destroy(c);
  CHECK(f.live == 0);
}

static void test_entry_allocation_failures(void) {
  for (size_t budget = 0; budget < 2; budget++) {
    fail_ctx f = {.remaining = 64};
    zmap_alloc a = {&f, fail_alloc, fail_dealloc};
    zlru *c = zlru_create(1, record_destroy, NULL, a);
    CHECK(c != NULL);
    if (!c) return;
    CHECK(zlru_put(c, "kept", VP(1)));
    size_t before = f.live;
    destroyed_calls = 0;
    f.remaining = budget;
    CHECK(!zlru_put(c, "refused", VP(2)));
    CHECK(zlru_size(c) == 1 && zlru_get(c, "kept") == VP(1));
    CHECK(zlru_get(c, "refused") == NULL);
    CHECK(f.live == before && destroyed_calls == 0);
    f.remaining = 2;
    CHECK(zlru_put(c, "replacement", VP(3)));
    CHECK(zlru_get(c, "kept") == NULL && destroyed_calls == 1);
    zlru_destroy(c);
    CHECK(f.live == 0 && destroyed_calls == 2);
  }
}

static bool check_growth_entry(void *ctx, const char *key, void *value) {
  int *expected = ctx;
  CHECK(*expected > 0);
  if (*expected <= 0) return false;
  char wanted[2] = {(char)('a' + *expected - 1), '\0'};
  CHECK(strcmp(key, wanted) == 0);
  CHECK(value == VP(*expected));
  (*expected)--;
  return true;
}

static void test_growth_allocation_failures(void) {
  for (size_t budget = 0; budget < 3; budget++) {
    fail_ctx f = {.remaining = 100};
    zmap_alloc a = {&f, fail_alloc, fail_dealloc};
    zlru *c = zlru_create(11, record_destroy, NULL, a);
    CHECK(c != NULL);
    if (!c) return;
    for (int i = 0; i < 11; i++) {
      char key[2] = {(char)('a' + i), '\0'};
      CHECK(zlru_put(c, key, VP(i + 1)));
    }
    size_t before = f.live;
    destroyed_calls = 0;
    f.remaining = budget;
    CHECK(!zlru_put(c, "grow", VP(12)));
    CHECK(zlru_size(c) == 11 && f.live == before);
    CHECK(zlru_get(c, "grow") == NULL && destroyed_calls == 0);
    int expected = 11;
    zlru_visit_mru_first(c, check_growth_entry, &expected);
    CHECK(expected == 0); /* A refused put cannot reorder or lose entries. */
    for (int i = 0; i < 11; i++) {
      char key[2] = {(char)('a' + i), '\0'};
      CHECK(zlru_get(c, key) == VP(i + 1));
    } /* Ascending lookups leave the original recency order in place. */
    f.remaining = 3;
    CHECK(zlru_put(c, "grow", VP(12)));
    CHECK(zlru_get(c, "a") == NULL && destroyed_calls == 1);
    CHECK(zlru_get(c, "grow") == VP(12));
    zlru_destroy(c);
    CHECK(f.live == 0 && destroyed_calls == 12);
  }
}

static void test_key_sizes(void) {
  static char long_key[4097];
  memset(long_key, 'k', sizeof(long_key) - 1);
  fail_ctx f = {.remaining = 64};
  zmap_alloc a = {&f, fail_alloc, fail_dealloc};
  zlru *c = zlru_create(2, NULL, NULL, a);
  CHECK(c != NULL);
  if (!c) return;
  CHECK(zlru_put(c, "", VP(1)));
  CHECK(zlru_put(c, long_key, VP(2)));
  f.remaining = 0;
  CHECK(zlru_put(c, long_key, VP(3))); /* Existing keys need no allocation. */
  CHECK(zlru_get(c, "") == VP(1));
  CHECK(zlru_get(c, long_key) == VP(3));
  zlru_destroy(c);
  CHECK(f.live == 0);
}

static void test_null_safety(void) {
  zlru_destroy(NULL);
  CHECK(zlru_get(NULL, "k") == NULL);
  CHECK(!zlru_put(NULL, "k", VP(1)));
  zlru_erase(NULL, "k");
  CHECK(zlru_size(NULL) == 0);
  CHECK(zlru_capacity(NULL) == 0);
  zlru_visit_mru_first(NULL, collect_key, NULL);
  zlru *c = zlru_create(2, NULL, NULL, (zmap_alloc){0});
  CHECK(c);
  CHECK(zlru_get(c, NULL) == NULL);
  CHECK(!zlru_put(c, NULL, VP(1)));
  zlru_erase(c, NULL);
  zlru_visit_mru_first(c, NULL, NULL);
  zlru_destroy(c);
}

static bool count_visit(void *ctx, const char *key, void *value) {
  (void)key;
  (void)value;
  (*(size_t *)ctx)++;
  return true;
}

static void test_stress(void) {
  /* Churn: 10k puts (211 distinct keys) over capacity 97 with periodic
   * gets and erases. Invariants: size never exceeds capacity, the list
   * walk visits exactly size() nodes, and every inserted value is
   * destroyed exactly once (replace, eviction, erase, and final
   * destroy are the only exits). */
  destroyed_len = 0;
  destroyed_calls = 0;
  destroyed_calls = 0;
  zlru *c = zlru_create(97, record_destroy, NULL, (zmap_alloc){0});
  CHECK(c);
  for (int i = 0; i < 10000; i++) {
    char key[32];
    int k = (int)((unsigned)i * 2654435761u % 211);
    snprintf(key, sizeof(key), "k%d", k);
    CHECK(zlru_put(c, key, VP(i + 1)));
    if (i % 13 == 0)
      zlru_get(c, "k42");
    if (i % 17 == 0)
      zlru_erase(c, "k7");
    CHECK(zlru_size(c) <= 97);
    size_t walked = 0;
    zlru_visit_mru_first(c, count_visit, &walked);
    CHECK(walked == zlru_size(c));
  }
  size_t final_size = zlru_size(c);
  zlru_destroy(c);
  CHECK(destroyed_calls == 10000); /* every value destroyed once */
  CHECK(final_size <= 97);
}

int main(void) {
  test_basic();
  test_eviction_order();
  test_replace();
  test_erase();
  test_capacity_one();
  test_visit_order();
  test_visit_early_stop();
  test_alloc_failure();
  test_entry_key_lifetime();
  test_entry_allocation_failures();
  test_growth_allocation_failures();
  test_key_sizes();
  test_null_safety();
  test_stress();
  if (failures) {
    fprintf(stderr, "test_zlru: %d failure(s)\n", failures);
    return 1;
  }
  puts("test_zlru: all tests passed");
  return 0;
}
