/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * purpose: z23-fixed-result-signer, run as z23verify (UID 60092).
 *
 *   pubkey    print the installed seed's public key for root to pin
 *   seal-fds  seal the launch handed over as descriptors 3..7: launch.bin,
 *             object.o, deps.d, stderr.bin, preprocessed.i (a failure
 *             receipt leaves 4 and 5 closed)
 *
 * It executes nothing and takes no path, UID or key from its arguments or
 * environment; all trust comes from root-owned /etc/z23verify. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "verify/fixed_result_signer.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int refuse(const char *why)
{
    fprintf(stderr, "fixed_result_signer_refuse=%s\n",
            why ? why : ZCL_FRS_WHY_ARGUMENTS);
    return 2;
}

static int pubkey_main(void)
{
    char hex[65];
    const char *why = zcl_frs_pubkey_production(hex);
    if (why) return refuse(why);
    printf("%s\n", hex);
    return fflush(stdout) == 0 ? 0 : refuse("signer_output_failed");
}

static int seal_fds_main(void)
{
    int fds[ZCL_FRS_INPUTS];
    for (int i = 0; i < ZCL_FRS_INPUTS; i++)
        fds[i] = fcntl(3 + i, F_GETFD) >= 0 ? 3 + i : -1;
    struct zcl_frs_result r;
    zcl_frs_seal_production(fds, &r);
    if (r.reason) return refuse(r.reason);
    printf("fixed_result_signer_sealed=1 verdict=%s exit_code=%d "
           "store_key=%s record_sha3=%s\n",
           r.failure ? "fail" : "pass", (int)r.exit_code, r.store_key,
           r.record_sha3);
    return fflush(stdout) == 0 ? 0 : refuse("signer_output_failed");
}

int main(int argc, char **argv)
{
    umask(0077);
    if (argc == 2 && strcmp(argv[1], "pubkey") == 0) return pubkey_main();
    if (argc == 2 && strcmp(argv[1], "seal-fds") == 0) return seal_fds_main();
    return refuse("request_shape_unsupported");
}
