/**
 * @file menu_input.c
 * @brief ATC Menu - key handler, line editor, CSI swallower.
 * @author Ahmet Talha ARDIC
 * @date   2026-07-31
 */
#include "menu_internal.h"

#define ESC_NONE 0
#define ESC_GOT  1  /* got ESC, waiting for '[' or 'O' */
#define ESC_CSI  2  /* inside sequence, waiting for final byte 0x40..0x7E */

/* The byte is handled the moment it arrives - there is no queue to drain, so
 * nothing can be dropped and nothing waits for the next atc_menu_update().
 * The mask matters on C2000, where a `char` is 16 bits wide. */
void atc_menu_key(atc_menu_ctx_t *ctx, char byte)
{
    unsigned char c = (unsigned char)(byte & 0xFF);

    if (ctx->esc_state == ESC_GOT) {
        ctx->esc_state = (c == '[' || c == 'O') ? ESC_CSI : ESC_NONE;
        return;
    }
    if (ctx->esc_state == ESC_CSI) {
        if (c >= 0x40 && c <= 0x7E)
            ctx->esc_state = ESC_NONE;
        return;
    }
    if (c == 0x1B) {
        if (ctx->mode != ATC_MENU_MODE_SELECT) {
            /* ESC cancels the edit/choice-preview immediately. The cost: an
             * escape sequence (arrow key) sent while editing also cancels;
             * its tail bytes are harmless in select mode. */
            ctx->input_len = 0;
            ctx->mode = ATC_MENU_MODE_SELECT;
            ctx->msg = 0;
            ctx->prompt_pos = 0;
        } else {
            ctx->esc_state = ESC_GOT;
        }
        return;
    }

    if (c == '\n' && ctx->last_cr) {  /* swallow LF of CRLF */
        ctx->last_cr = 0;
        return;
    }
    ctx->last_cr = (c == '\r');

    ctx->msg = 0;

    if (c == '\r' || c == '\n') {
        atc_menu_enter(ctx);
    } else if (c == 0x03) {  /* Ctrl-C: cancel edit / clear input */
        ctx->input_len = 0;
        ctx->mode = ATC_MENU_MODE_SELECT;
    } else if (c == 0x08 || c == 0x7F) {
        if (ctx->input_len)
            ctx->input_len--;
    } else if (c >= 0x20 && c <= 0x7E) {
        if (ctx->mode == ATC_MENU_MODE_SELECT) {
            if (c >= '0' && c <= '9') {
                if (c == '0' && ctx->input_len == 0) {
                    atc_menu_cmd(ctx, '0');
                } else if (ctx->input_len < ATC_MENU_CFG_INPUT_BUF - 1) {
                    ctx->input[ctx->input_len++] = (char)c;
                    atc_menu_select_try(ctx);  /* acts once unambiguous */
                }
            } else if (ctx->input_len == 0 &&
                       (c == 'b' || c == 'r' || c == '?' ||
                        c == 'n' || c == 'p' || c == 'i')) {
                atc_menu_cmd(ctx, (char)c);
            }
            /* anything else is ignored in select mode */
        } else if (ctx->mode == ATC_MENU_MODE_CHOICE) {
            atc_menu_choice_next(ctx);
        } else if (ctx->input_len < ATC_MENU_CFG_INPUT_BUF - 1) {
            ctx->input[ctx->input_len++] = (char)c;
        }
    } else {
        return;  /* other control bytes: no prompt redraw */
    }

    ctx->prompt_pos = 0;  /* echo */
}
