/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "os.h"
#include "os_io_seproxyhal.h"
#include "blue_review_app.h"
#include "blue_review_layout.h"
#include <string.h>

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error "The Ledger Blue app requires ISO C23"
#endif

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];
ux_state_t ux;
static blue_review_app review_app;
static cx_blake2b_t zip_context;

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
            .type = BAGL_RECTANGLE, .x = 0, .y = ZCL_BLUE_HEADER_HEIGHT,
            .width = ZCL_BLUE_SCREEN_WIDTH,
            .height = ZCL_BLUE_SCREEN_HEIGHT - ZCL_BLUE_HEADER_HEIGHT,
            .fill = BAGL_FILL,
            .fgcolor = ZCL_BLUE_COLOR_BODY,
            .bgcolor = ZCL_BLUE_COLOR_BODY
        }
    },
    {
        .component = {
            .type = BAGL_RECTANGLE, .x = 0, .y = 0,
            .width = ZCL_BLUE_SCREEN_WIDTH,
            .height = ZCL_BLUE_HEADER_HEIGHT, .fill = BAGL_FILL,
            .fgcolor = ZCL_BLUE_COLOR_HEADER,
            .bgcolor = ZCL_BLUE_COLOR_HEADER
        }
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_HEADER_TEXT_X, .y = 0,
            .width = ZCL_BLUE_HEADER_TEXT_WIDTH,
            .height = ZCL_BLUE_HEADER_HEIGHT, .fill = BAGL_FILL,
            .fgcolor = ZCL_BLUE_COLOR_WHITE,
            .bgcolor = ZCL_BLUE_COLOR_HEADER,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_MIDDLE
        },
        .text = "ZCL Review"
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[0]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y + ZCL_BLUE_LINE_STEP_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[1]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y + 2 * ZCL_BLUE_LINE_STEP_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[2]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y + 3 * ZCL_BLUE_LINE_STEP_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[3]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y + 4 * ZCL_BLUE_LINE_STEP_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[4]
    },
    {
        .component = {
            .type = BAGL_LABEL, .x = ZCL_BLUE_LINE_X,
            .y = ZCL_BLUE_LINE_FIRST_Y + 5 * ZCL_BLUE_LINE_STEP_Y,
            .width = ZCL_BLUE_LINE_WIDTH,
            .height = ZCL_BLUE_LINE_HEIGHT,
            .fgcolor = ZCL_BLUE_COLOR_TEXT,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER
        },
        .text = review_app.lines[5]
    },
    {
        .component = {
            .type = BAGL_BUTTON | BAGL_FLAG_TOUCHABLE,
            .x = ZCL_BLUE_NEXT_X, .y = ZCL_BLUE_BUTTON_Y,
            .width = ZCL_BLUE_BUTTON_WIDTH,
            .height = ZCL_BLUE_BUTTON_HEIGHT,
            .radius = 6, .fill = BAGL_FILL,
            .fgcolor = ZCL_BLUE_COLOR_BUTTON,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
            .font_id = BAGL_FONT_OPEN_SANS_LIGHT_14px |
                       BAGL_FONT_ALIGNMENT_CENTER |
                       BAGL_FONT_ALIGNMENT_MIDDLE
        },
        .text = "NEXT PAGE", .tap = show_latest
    },
    {
        .component = {
            .type = BAGL_BUTTON | BAGL_FLAG_TOUCHABLE,
            .x = ZCL_BLUE_EXIT_X, .y = ZCL_BLUE_BUTTON_Y,
            .width = ZCL_BLUE_BUTTON_WIDTH,
            .height = ZCL_BLUE_BUTTON_HEIGHT,
            .radius = 6, .fill = BAGL_FILL,
            .fgcolor = ZCL_BLUE_COLOR_BUTTON,
            .bgcolor = ZCL_BLUE_COLOR_BODY,
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
    blue_review_app_next(&review_app, transaction_digest);
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
                zcl_zip243_hasher hasher = {
                    .context = &zip_context, .init = zip243_start,
                    .update = zip243_update, .final = zip243_finish
                };
                sw = blue_review_app_command(&review_app, G_io_apdu_buffer,
                    rx, G_io_apdu_buffer, sizeof G_io_apdu_buffer - 2,
                    &reply_length, transaction_digest, &hasher);
                tx = reply_length;
            }
            CATCH_OTHER(error) {
                sw = (error & 0xf000) == 0x6000 ||
                     (error & 0xf000) == 0x9000
                         ? error : (0x6800 | (error & 0x07ff));
                review_app.transaction.expected = 0;
                review_app.transaction.received = 0;
                review_app.transaction.reviewed_length = 0;
                blue_review_app_reset(&review_app);
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
            blue_review_app_reset(&review_app);
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
