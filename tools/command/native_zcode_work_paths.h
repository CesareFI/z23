/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Purpose: one stable task-local workspace path for Zcode command consumers. */
#ifndef ZCL_NATIVE_ZCODE_WORK_PATHS_H
#define ZCL_NATIVE_ZCODE_WORK_PATHS_H

#include <stdbool.h>

#define ZWORK_TASK_PATH_MAX 4400

bool zwork_task_path(char out[ZWORK_TASK_PATH_MAX], const char *task,
                     const char *suffix);
#if !defined(_WIN32)
bool zwork_task_path_select(char out[ZWORK_TASK_PATH_MAX],
                            const char *legacy, const char *current,
                            const char *suffix);
#endif

#endif
