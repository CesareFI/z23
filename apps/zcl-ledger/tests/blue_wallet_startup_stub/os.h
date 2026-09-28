/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_BLUE_WALLET_STARTUP_OS_H
#define ZCL_BLUE_WALLET_STARTUP_OS_H

#include "../blue_wallet_sdk_stub/os.h"
#include <stdlib.h>

enum {
    CX_CURVE_256K1 = 3, CX_RIPEMD160 = 4,
    IO_SEPROXYHAL_BUFFER_SIZE_B = 128,
    CHANNEL_SPI = 1, CHANNEL_APDU = 2,
    IO_RETURN_AFTER_TX = 4, IO_FLAGS = 4,
    INVALID_PARAMETER = 1
};

typedef struct { int header; } cx_ripemd160_t;
typedef struct { uint8_t unused; } ux_state_t;
extern unsigned char G_io_apdu_buffer[260];

int os_global_pin_is_validated(void);
void os_perso_derive_node_bip32(unsigned curve, const unsigned *path,
    unsigned length, uint8_t raw[32], uint8_t chain[32]);
int cx_ecfp_init_private_key(unsigned curve, const uint8_t raw[32],
    unsigned length, cx_ecfp_private_key_t *key);
int cx_ecfp_generate_pair(unsigned curve, cx_ecfp_public_key_t *public_key,
    cx_ecfp_private_key_t *private_key, int keepprivate);
int cx_ripemd160_init(cx_ripemd160_t *context);
int cx_hash5(void *context, unsigned mode, const uint8_t *bytes,
    unsigned length, uint8_t *digest);
#define cx_hash(context, mode, bytes, length, digest) \
    cx_hash5(context, mode, bytes, length, digest)
void os_boot(void);
unsigned short io_exchange(unsigned char channel, unsigned short tx_length);

#define BEGIN_TRY if (1)
#define TRY if (1)
#define CATCH_OTHER(error) else for (unsigned error = 0; error < 1; ++error)
#define FINALLY if (1)
#define END_TRY
#define CLOSE_TRY ((void)0)
#define THROW(error) abort()

#endif
