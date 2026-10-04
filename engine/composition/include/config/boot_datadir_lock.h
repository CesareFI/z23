/* Copyright 2026 Rhett Creighton - Apache License 2.0 */

#ifndef ZCL_CONFIG_BOOT_DATADIR_LOCK_H
#define ZCL_CONFIG_BOOT_DATADIR_LOCK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Single-process guard for the selected datadir.
 *
 * Acquisition holds nonblocking OS file locks until release. On POSIX, the
 * wallet-recovery writer guard is acquired before the PID file so node start
 * cannot race restore or mnemonic recovery into the same stores. The lock
 * files are retained after release; leaving their inodes in place avoids an
 * unlink/recreate race that could admit two writers. Any failure to establish
 * or durably record the locks fails closed. Successful acquisition arms the
 * first boot-status beacon; refusal leaves the owner's beacon. */
bool boot_datadir_lock_acquire(const char *datadir);
void boot_datadir_lock_release(void);

#ifdef __cplusplus
}
#endif

#endif /* ZCL_CONFIG_BOOT_DATADIR_LOCK_H */
