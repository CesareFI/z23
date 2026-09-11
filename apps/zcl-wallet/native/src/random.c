/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_keys.h"

#include <string.h>
#if defined(__linux__) || defined(__ANDROID__)
#include <errno.h>
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__)
#include <unistd.h>
#endif

static zcl_status fill_random(uint8_t *output, size_t length)
{
#if defined(__linux__) || defined(__ANDROID__)
    size_t offset = 0;
    for (size_t attempt = 0; attempt < 128 && offset < length; ++attempt) {
        ssize_t count = getrandom(output + offset, length - offset, GRND_NONBLOCK);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return ZCL_IO_FAILURE;
        }
        if (count == 0 || (size_t)count > length - offset)
            return ZCL_IO_FAILURE;
        offset += (size_t)count;
    }
    return offset == length ? ZCL_OK : ZCL_IO_FAILURE;
#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__)
    return getentropy(output, length) == 0 ? ZCL_OK : ZCL_IO_FAILURE;
#else
    (void)output;
    (void)length;
    return ZCL_UNSUPPORTED;
#endif
}

zcl_status zcl_random_bytes(uint8_t *output, size_t output_length)
{
    if (output == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (output_length == 0 || output_length > 64)
        return ZCL_OUT_OF_RANGE;
    uint8_t scratch[64] = {0};
    zcl_status status = fill_random(scratch, output_length);
    if (status == ZCL_OK)
        memcpy(output, scratch, output_length);
    zcl_secure_zero(scratch, sizeof(scratch));
    return status;
}
