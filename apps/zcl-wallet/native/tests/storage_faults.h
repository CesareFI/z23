/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_STORAGE_FAULTS_H
#define ZCL_STORAGE_FAULTS_H
#include <stddef.h>

typedef enum { IO_NORMAL, IO_SHORT, IO_INTERRUPT, IO_ZERO, IO_ERROR, IO_OVERSIZE, IO_COLLISION,
    IO_PARTIAL_ERROR } io_mode;
typedef struct { io_mode mode; size_t at; size_t calls; } io_fault;
extern io_fault storage_read_fault, storage_write_fault, storage_sync_fault;
extern io_fault storage_rename_fault, storage_close_fault;
extern io_fault storage_pread_fault;
void storage_faults_reset(void);
#endif
