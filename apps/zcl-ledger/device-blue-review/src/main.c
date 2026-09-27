/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "os_io_seproxyhal.h"
#include "blue_review_protocol.h"
#include "blue_review_screen.h"
#include <string.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "The Ledger Blue app requires ISO C23"
#endif

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];
ux_state_t ux;
static blue_review_state review_state;
static char review_lines[ZCL_BLUE_REVIEW_LINES][ZCL_BLUE_REVIEW_LINE_SIZE];
static uint8_t review_reply[76];
static uint32_t review_page;

static void clear_review_screen(void) {
    review_page = 0;
    memset(review_reply, 0, sizeof review_reply);
    strcpy(review_lines[0], "CONNECT Z23");
    strcpy(review_lines[1], "SEND A TRANSACTION");
    strcpy(review_lines[2], "TAP NEXT PAGE TO VIEW");
    review_lines[3][0] = 0;
    strcpy(review_lines[4], "READ ONLY; NO SIGNING");
    review_lines[5][0] = 0;
}

static bool transaction_digest(const uint8_t *wire, size_t length,
                               uint8_t digest[32]) {
    return cx_hash_sha256(wire, (unsigned int)length, digest) == 32;
}

static bool zip243_start(void *context, const uint8_t personal[16]) {
    uint8_t mutable_personal[16];
    memcpy(mutable_personal, personal, sizeof mutable_personal);
    return cx_blake2b_init2(context, 256, NULL, 0,
                             mutable_personal, sizeof mutable_personal) ==
                             CX_BLAKE2B;
}

static bool zip243_update(void *context, const uint8_t *bytes, size_t length) {
    return (cx_hash)(context, 0, bytes, (unsigned int)length, NULL, 0) >= 0;
}

static bool zip243_finish(void *context, uint8_t digest[32]) {
    return (cx_hash)(context, CX_LAST, NULL, 0, digest, 32) == 32;
}

static const bagl_element_t *exit_app(const bagl_element_t *element) {
    (void)element;
    os_sched_exit(0);
    return NULL;
}

static const bagl_element_t *show_latest(const bagl_element_t *element);

static unsigned int review_ui_button(unsigned int button_mask,
                                     unsigned int button_mask_counter) {
    (void)button_mask;
    (void)button_mask_counter;
    return 0;
}

static const bagl_element_t review_ui[] = {
    {
        .component = {
            .type = BAGL_RECTANGLE, .x = 0, .y = 60, .width = 320,
            .height = 420, .fill = BAGL_FILL,
            .fgcolor = 0xf9f9f9, .bgcolor = 0xf9f9f9
        }
    },
    {
        .component = {
            .type = BAGL_RECTANGLE, .x = 0, .y = 0, .width = 320,
            .height = 60, .fill = BAGL_FILL,
            .fgcolor = 0x1d2028, .bgcolor = 0x1d2028
        }
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 20, .y = 0, .width = 280,
            .height = 60, .fill = BAGL_FILL, .fgcolor = 0xffffff,
            .bgcolor = 0x1d2028,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_MIDDLE
        },
        .text = "ZCL Review"
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 80, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[0]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 125, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[1]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 170, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[2]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 215, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[3]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 260, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[4]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = 10, .y = 305, .width = 300,
            .height = 40, .fgcolor = 0x1d2028, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_lines[5]
    },
    {
        .component = {
            .type = BAGL_BUTTON | BAGL_FLAG_TOUCHABLE,
            .x = 20, .y = 390, .width = 130, .height = 40,
            .radius = 6, .fill = BAGL_FILL,
            .fgcolor = 0x41ccb4, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER |
                       BAGL_FONT_ALIGNMENT_MIDDLE
        },
        .text = "NEXT PAGE", .tap = show_latest
    },
    {
        .component = {
            .type = BAGL_BUTTON | BAGL_FLAG_TOUCHABLE,
            .x = 170, .y = 390, .width = 130, .height = 40,
            .radius = 6, .fill = BAGL_FILL,
            .fgcolor = 0x41ccb4, .bgcolor = 0xf9f9f9,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER |
                       BAGL_FONT_ALIGNMENT_MIDDLE
        },
        .text = "EXIT",
        .overfgcolor = 0x37ae99,
        .overbgcolor = 0xf9f9f9,
        .tap = exit_app
    }
};

static const bagl_element_t *show_latest(const bagl_element_t *element) {
    (void)element;
    if (review_state.expected || !review_state.reviewed_length) {
        clear_review_screen();
        UX_DISPLAY(review_ui, NULL);
        return NULL;
    }
    uint32_t count = (uint32_t)review_reply[4] |
        ((uint32_t)review_reply[5] << 8) |
        ((uint32_t)review_reply[6] << 16) |
        ((uint32_t)review_reply[7] << 24);
    uint32_t next = review_page;
    bool formatted = next == 0 ?
        blue_review_screen_format(review_reply, review_lines) :
        blue_review_screen_output(review_state.wire,
                                  review_state.reviewed_length, next - 1,
                                  transaction_digest, review_lines);
    if (!formatted) {
        clear_review_screen();
        review_state.reviewed_length = 0;
        UX_DISPLAY(review_ui, NULL);
        return NULL;
    }
    review_page = next >= count ? 0 : next + 1;
    UX_DISPLAY(review_ui, NULL);
    return NULL;
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    if ((channel & ~IO_FLAGS) != CHANNEL_SPI) THROW(INVALID_PARAMETER);
    if (tx_len != 0) {
        io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);
        return 0;
    }
    return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                 sizeof G_io_apdu_buffer, 0);
}

void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
    (void)channel;
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;
    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;
    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if (!UX_DISPLAYED()) UX_DISPLAYED_EVENT();
        break;
    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, (void)0;);
        break;
    default:
        break;
    }
    if (!io_seproxyhal_spi_is_status_sent()) io_seproxyhal_general_status();
    return 1;
}

static void answer_command(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    for (;;) {
        volatile unsigned short sw = 0x6f00;
        BEGIN_TRY {
            TRY {
                rx = io_exchange(CHANNEL_APDU, tx);
                tx = 0;
                size_t reply_length = 0;
                uint8_t instruction = rx >= 2 ? G_io_apdu_buffer[1] : 0;
                cx_blake2b_t zip_context;
                zcl_zip243_hasher hasher = {
                    .context = &zip_context, .init = zip243_start,
                    .update = zip243_update, .final = zip243_finish
                };
                sw = blue_review_handle(&review_state, G_io_apdu_buffer,
                                        rx, G_io_apdu_buffer,
                                        sizeof G_io_apdu_buffer - 2,
                                        &reply_length, transaction_digest,
                                        &hasher);
                if (instruction == 0x10 || instruction == 0x13 ||
                    (instruction == 0x11 && sw != 0x9000))
                    clear_review_screen();
                if (instruction == 0x12) {
                    if (sw != 0x9000 || reply_length != 76 ||
                        !blue_review_screen_format(G_io_apdu_buffer,
                                                   review_lines)) {
                        clear_review_screen();
                        if (sw == 0x9000) sw = 0x6a80;
                        reply_length = 0;
                    } else {
                        memcpy(review_reply, G_io_apdu_buffer,
                               sizeof review_reply);
                        review_page = 0;
                    }
                }
                tx = reply_length;
            }
            CATCH_OTHER(error) {
                sw = (error & 0xf000) == 0x6000 ||
                     (error & 0xf000) == 0x9000
                         ? error : (0x6800 | (error & 0x07ff));
                review_state.expected = review_state.received = 0;
                review_state.reviewed_length = 0;
                clear_review_screen();
            }
            FINALLY {}
        }
        END_TRY;
        G_io_apdu_buffer[tx++] = (unsigned char)(sw >> 8);
        G_io_apdu_buffer[tx++] = (unsigned char)sw;
    }
}

__attribute__((section(".boot"))) int main(void) {
    __asm volatile("cpsie i");
    os_boot();
    UX_INIT();
    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();
            USB_power(0);
            USB_power(1);
            clear_review_screen();
            UX_DISPLAY(review_ui, NULL);
            answer_command();
        }
        CATCH_OTHER(error) {
            (void)error;
        }
        FINALLY {}
    }
    END_TRY;
    return 0;
}
