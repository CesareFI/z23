/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
/* Writes public fuzz seeds only, into an explicit pre-created fixture directory. */
#include "scan_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool write_seed(const char *directory, unsigned int index, unsigned int rotation,
                        const uint8_t *text, size_t length, zcl_network network)
{
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/public-%u-%u", directory, index, rotation);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;
    zcl_qr_image layout = {0};
    size_t image_len = 0;
    uint8_t *image = scan_fixture(text, length, 3, rotation, 1, 0, &layout, &image_len);
    if (image == NULL) return false;
    uint8_t header[8] = {(uint8_t)(layout.width & 255), (uint8_t)(layout.width >> 8),
        (uint8_t)(layout.height & 255), (uint8_t)(layout.height >> 8),
        (uint8_t)(layout.row_stride & 255), (uint8_t)(layout.row_stride >> 8), 1,
        network == ZCL_MAINNET ? 1 : 0};
    FILE *file = fopen(path, "wb");
    bool ok = false;
    if (file != NULL) {
        ok = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
             fwrite(image, 1, image_len, file) == image_len;
        if (fclose(file) != 0) ok = false;
    }
    free(image);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    uint8_t hash[20], address[35];
    for (unsigned int index = 0; index < 16; ++index) {
        memset(hash, (int)(index * 17), sizeof(hash));
        size_t length = 0;
        zcl_network network = index % 2 == 0 ? ZCL_MAINNET : ZCL_TESTNET;
        if (zcl_address_from_hash(hash, sizeof(hash), network, address, sizeof(address), &length) != ZCL_OK)
            return 1;
        for (unsigned int rotation = 0; rotation < 4; ++rotation)
            if (!write_seed(argv[1], index, rotation, address, length, network)) return 1;
    }
    static const uint8_t uri[] = "zclassic:t1T8yaLVhNqxA5KJcmiqqFN88e8DNp2PBfF?amount=1.25&label=Public%20fixture";
    return write_seed(argv[1], 16, 0, uri, sizeof(uri) - 1, ZCL_MAINNET) ? 0 : 1;
}
