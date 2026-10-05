/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: zpool test suite.  Exits nonzero on the first failure. */
#include "zpool/zpool.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      failures++;                                                            \
    }                                                                        \
  } while (0)

static _Alignas(16) unsigned char arena[64 * 16]; /* 16 x 64-byte blocks */

static void test_init_validation(void) {
  zpool p;
  memset(arena, 0xAA, sizeof(arena));
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  CHECK(p.block_size == 64);
  CHECK(p.block_count == 16);
  CHECK(zpool_available(&p) == 16);
  /* Block size rounds up to hold a link and to align. */
  CHECK(zpool_init(&p, arena, sizeof(arena), 1));
  CHECK(p.block_size == _Alignof(max_align_t));
  /* Too-small arena, NULL args, unaligned arena. */
  CHECK(!zpool_init(&p, arena, 8, 64));
  CHECK(!zpool_init(&p, NULL, sizeof(arena), 64));
  CHECK(!zpool_init(NULL, arena, sizeof(arena), 64));
  CHECK(!zpool_init(&p, arena + 1, sizeof(arena) - 1, 64));
  /* Overflow-huge block size fails closed. */
  CHECK(!zpool_init(&p, arena, sizeof(arena), SIZE_MAX - 3));
  /* Leftover tail smaller than a block is simply unused. */
  CHECK(zpool_init(&p, arena, 64 + 32, 64));
  CHECK(p.block_count == 1);
}

static void test_alloc_exhaustion(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  void *blocks[16];
  for (int i = 0; i < 16; i++) {
    blocks[i] = zpool_alloc(&p);
    CHECK(blocks[i] != NULL);
    CHECK(((uintptr_t)blocks[i] % _Alignof(max_align_t)) == 0);
    CHECK(zpool_available(&p) == (size_t)(15 - i));
  }
  CHECK(zpool_alloc(&p) == NULL); /* exhausted */
  CHECK(zpool_alloc(NULL) == NULL);
  /* All distinct. */
  for (int i = 0; i < 16; i++)
    for (int j = i + 1; j < 16; j++)
      CHECK(blocks[i] != blocks[j]);
  for (int i = 0; i < 16; i++)
    CHECK(zpool_free(&p, blocks[i]));
  CHECK(zpool_available(&p) == 16);
}

static void test_reuse_and_lifo(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  void *a = zpool_alloc(&p);
  void *b = zpool_alloc(&p);
  CHECK(a && b);
  /* User writes through the whole block; free must still work. */
  memset(a, 0x55, 64);
  memset(b, 0x66, 64);
  CHECK(zpool_free(&p, a));
  CHECK(zpool_free(&p, b));
  void *c = zpool_alloc(&p); /* LIFO: b comes back first */
  CHECK(c == b);
  void *d = zpool_alloc(&p);
  CHECK(d == a);
  CHECK(zpool_free(&p, c));
  CHECK(zpool_free(&p, d));
}

static void test_free_validation(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  void *a = zpool_alloc(&p);
  CHECK(a);
  memset(a, 0x77, 64); /* scribble over the whole live block */
  unsigned char outside[64] = {0};
  CHECK(!zpool_free(&p, NULL));
  CHECK(!zpool_free(NULL, a));
  CHECK(!zpool_free(&p, outside)); /* out of arena */
  CHECK(!zpool_free(&p, arena + sizeof(arena))); /* one past the end */
  CHECK(!zpool_free(&p, (unsigned char *)a + 8)); /* misaligned */
  CHECK(zpool_free(&p, a)); /* the real free */
  CHECK(!zpool_free(&p, a)); /* double free rejected */
  /* A never-allocated block is on the free list: freeing it is a
   * double free and must be rejected. */
  void *never = arena + 64; /* still free: only block 0 was taken */
  CHECK(!zpool_free(&p, never));
  CHECK(zpool_available(&p) == 16);
}

static void test_owns(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  void *a = zpool_alloc(&p);
  CHECK(zpool_owns(&p, a));
  CHECK(!zpool_owns(&p, arena + 64)); /* in arena but free */
  CHECK(!zpool_owns(&p, (unsigned char *)a + 8));
  CHECK(!zpool_owns(&p, NULL));
  CHECK(!zpool_owns(NULL, a));
  CHECK(zpool_free(&p, a));
  CHECK(!zpool_owns(&p, a));
}

static void test_interleaved_churn(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  void *live[16] = {0};
  size_t live_n = 0;
  for (int round = 0; round < 2000; round++) {
    if (live_n && (round % 3 == 0)) {
      size_t i = (size_t)(round * 7) % live_n;
      CHECK(zpool_owns(&p, live[i]));
      CHECK(zpool_free(&p, live[i]));
      live[i] = live[--live_n];
    } else {
      void *b = zpool_alloc(&p);
      if (!b) {
        CHECK(live_n == 16);
        continue;
      }
      live[live_n++] = b;
      memset(b, round & 0xFF, 64);
    }
    CHECK(zpool_available(&p) == 16 - live_n);
    /* Verify every live block is owned and no two live blocks alias. */
    for (size_t i = 0; i < live_n; i++) {
      CHECK(zpool_owns(&p, live[i]));
      for (size_t j = i + 1; j < live_n; j++)
        CHECK(live[i] != live[j]);
    }
  }
  for (size_t i = 0; i < live_n; i++)
    CHECK(zpool_free(&p, live[i]));
  CHECK(zpool_available(&p) == 16);
}

static void test_small_blocks(void) {
  /* 8-byte requests still get aligned, link-sized blocks. */
  _Alignas(16) unsigned char small[4 * 16];
  zpool p;
  CHECK(zpool_init(&p, small, sizeof(small), 8));
  CHECK(p.block_size == 16);
  CHECK(p.block_count == 4);
  void *b[4];
  for (int i = 0; i < 4; i++)
    CHECK((b[i] = zpool_alloc(&p)) != NULL);
  CHECK(zpool_alloc(&p) == NULL);
  for (int i = 0; i < 4; i++)
    CHECK(zpool_free(&p, b[i]));
  CHECK(zpool_available(&p) == 4);
}

static void test_foreign_pointer_ownership(void) {
  unsigned char foreign[64] = {0};
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  /* Separate objects may not be compared or subtracted as C pointers. */
  CHECK(!zpool_owns(&p, foreign));
  CHECK(zpool_available(&p) == 16);
}

/* Refusal includes both the result and the header's no-state-change promise.
 * Stop before free on a bad owns result so validator mutations cannot write
 * through foreign, one-past, or NULL pointers. */
static void check_refusal(zpool *pool, void *ptr, const char *row) {
  unsigned char before[sizeof(arena)];
  memcpy(before, arena, sizeof(before));
  zpool saved = *pool;
  if (zpool_owns(pool, ptr)) {
    fprintf(stderr, "FAIL contract row %s: owns accepted\n", row);
    failures++;
    return;
  }
  bool refused = !zpool_free(pool, ptr);
  bool unchanged = pool->arena == saved.arena &&
                   pool->block_size == saved.block_size &&
                   pool->block_count == saved.block_count &&
                   pool->free_count == saved.free_count &&
                   pool->free_head == saved.free_head &&
                   memcmp(before, arena, sizeof(before)) == 0;
  if (!refused || !unchanged) {
    fprintf(stderr, "FAIL contract row %s: free refusal/state\n", row);
    failures++;
  }
}

static void test_contract_rows(void) {
  zpool p;
  CHECK(zpool_init(&p, arena, sizeof(arena), 64));
  check_refusal(&p, arena, "never allocated");
  void *blocks[16];
  for (size_t i = 0; i < 16; i++) {
    blocks[i] = zpool_alloc(&p);
    if (blocks[i] != arena + i * p.block_size ||
        !zpool_owns(&p, blocks[i])) {
      fprintf(stderr, "FAIL contract row live starts: block %zu\n", i);
      failures++;
    }
  }
  CHECK(32 % _Alignof(max_align_t) == 0);
  check_refusal(&p, arena + 32, "aligned interior");
  check_refusal(&p, arena + 1, "misaligned interior");
  check_refusal(&p, arena + sizeof(arena), "one past");
  unsigned char foreign[64] = {0};
  unsigned char saved_foreign[sizeof(foreign)] = {0};
  check_refusal(&p, foreign, "foreign");
  CHECK(memcmp(foreign, saved_foreign, sizeof(foreign)) == 0);
  check_refusal(&p, NULL, "null");
  for (size_t i = 0; i < 16; i++) {
    CHECK(zpool_free(&p, blocks[i]));
    CHECK(zpool_available(&p) == i + 1);
    check_refusal(&p, blocks[i], "freed");
  }
}

int main(void) {
  test_contract_rows();
  test_foreign_pointer_ownership();
  test_init_validation();
  test_alloc_exhaustion();
  test_reuse_and_lifo();
  test_free_validation();
  test_owns();
  test_interleaved_churn();
  test_small_blocks();
  if (failures) {
    fprintf(stderr, "test_zpool: %d failure(s)\n", failures);
    return 1;
  }
  puts("test_zpool: all tests passed");
  return 0;
}
