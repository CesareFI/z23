/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: bounded LRU cache on zmap (see the header for the design). */
#include "zlru/zlru.h"

#include <string.h>

typedef struct zlru_node {
  struct zlru_node *prev, *next; /* MRU ... LRU */
  void *value;
  char key[];                    /* node-owned trailing copy */
} zlru_node;

struct zlru {
  zmap *map;
  zlru_node *head, *tail;
  size_t size, capacity;
  zlru_destroy_fn dtor;
  void *dtor_ctx;
  zmap_alloc alloc;
};

static void unlink_node(zlru *c, zlru_node *n) {
  if (n->prev)
    n->prev->next = n->next;
  else
    c->head = n->next;
  if (n->next)
    n->next->prev = n->prev;
  else
    c->tail = n->prev;
  n->prev = n->next = NULL;
}

static void push_head(zlru *c, zlru_node *n) {
  n->prev = NULL;
  n->next = c->head;
  if (c->head)
    c->head->prev = n;
  else
    c->tail = n;
  c->head = n;
}

static void free_node(zlru *c, zlru_node *n) {
  c->alloc.dealloc(c->alloc.ctx, n);
}

static zlru_node *create_node(zlru *c, const char *key, void *value) {
  size_t klen = strlen(key);
  if (klen > SIZE_MAX - sizeof(zlru_node) - 1)
    return NULL;
  zlru_node *n = c->alloc.alloc(c->alloc.ctx, sizeof(*n) + klen + 1);
  if (!n)
    return NULL;
  /* The node keeps its key alive after zmap releases its independent copy. */
  memcpy(n->key, key, klen + 1);
  n->value = value;
  return n;
}

zlru *zlru_create(size_t capacity, zlru_destroy_fn destroy_fn,
                  void *destroy_ctx, zmap_alloc allocator) {
  if (!capacity)
    return NULL;
  if (!allocator.alloc)
    allocator = zmap_alloc_malloc();
  zlru *c = allocator.alloc(allocator.ctx, sizeof(*c));
  if (!c)
    return NULL;
  c->map = zmap_create_ex(&allocator, NULL);
  if (!c->map) {
    allocator.dealloc(allocator.ctx, c);
    return NULL;
  }
  c->capacity = capacity;
  c->dtor = destroy_fn;
  c->dtor_ctx = destroy_ctx;
  c->alloc = allocator;
  return c;
}

static void node_dtor(void *ctx, const char *key, void *value) {
  /* zmap_clear/zmap_destroy entry destructor: value is the node. */
  zlru *c = ctx;
  zlru_node *n = value;
  (void)key; /* map-owned copy; released by zmap itself */
  if (c->dtor)
    c->dtor(c->dtor_ctx, n->key, n->value);
  free_node(c, n);
}

void zlru_destroy(zlru *c) {
  if (!c)
    return;
  zmap_destroy(c->map, node_dtor, c);
  zmap_alloc alloc = c->alloc;
  alloc.dealloc(alloc.ctx, c);
}

void *zlru_get(zlru *c, const char *key) {
  if (!c || !key)
    return NULL;
  zlru_node *n = zmap_get(c->map, key);
  if (!n)
    return NULL;
  unlink_node(c, n);
  push_head(c, n);
  return n->value;
}

/* Drop the LRU entry; the caller already checked size == capacity. */
static void evict_tail(zlru *c) {
  zlru_node *victim = c->tail;
  zmap_erase(c->map, victim->key); /* frees the map's key copy */
  unlink_node(c, victim);
  if (c->dtor)
    c->dtor(c->dtor_ctx, victim->key, victim->value);
  free_node(c, victim);
  c->size--;
}

bool zlru_put(zlru *c, const char *key, void *value) {
  if (!c || !key)
    return false;
  zlru_node *n = zmap_get(c->map, key);
  if (n) { /* replace: no allocation, destructor on the old value */
    if (c->dtor)
      c->dtor(c->dtor_ctx, n->key, n->value);
    n->value = value;
    unlink_node(c, n);
    push_head(c, n);
    return true;
  }

  n = create_node(c, key, value);
  if (!n)
    return false;
  if (!zmap_put(c->map, key, n, NULL)) {
    free_node(c, n);
    return false; /* map unchanged */
  }
  push_head(c, n);
  c->size++;
  if (c->size > c->capacity)
    evict_tail(c);
  return true;
}

void zlru_erase(zlru *c, const char *key) {
  if (!c || !key)
    return;
  zlru_node *n = zmap_erase(c->map, key);
  if (!n)
    return;
  unlink_node(c, n);
  if (c->dtor)
    c->dtor(c->dtor_ctx, n->key, n->value);
  free_node(c, n);
  c->size--;
}

size_t zlru_size(const zlru *c) { return c ? c->size : 0; }

size_t zlru_capacity(const zlru *c) { return c ? c->capacity : 0; }

void zlru_visit_mru_first(const zlru *c, zlru_visit_fn visit, void *ctx) {
  if (!c || !visit)
    return;
  /* The callback must not mutate the cache. */
  for (zlru_node *n = c->head; n; n = n->next) {
    if (!visit(ctx, n->key, n->value))
      return;
  }
}
