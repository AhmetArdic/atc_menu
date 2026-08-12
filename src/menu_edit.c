/* SPDX-License-Identifier: MIT */
/**
 * @file menu_edit.c
 * @brief The editor's state and its slice of the caller's buffer
 *
 * One editor at a time, and it lives past the frame that opened it: the title
 * and the keystrokes sit behind the row half of the caller's buffer, so a
 * label and a value need only stay valid for the widget call that showed them.
 *
 * F_EDIT says an editor is open; F_TEXT and F_CHOICE say which of the three it
 * is. The number editor is the one with no flag of its own.
 */
#include "menu_internal.h"

#include <string.h>

/*---------------------------------------------------------------------------
 * The title
 *-------------------------------------------------------------------------*/

/* Half the scratch, less the terminator: what a title may take before it starts
   eating into what the user can type. */
static size_t title_max(const atc_menu_ctx_t *c)
{
    return edit_room(c) / 2u - 1u;
}

/* "Freq (Hz) [1000]" — the prompt says which row it belongs to and what the row
   held when it opened, so a value can be checked against the old one without
   leaving the editor. Both strings belong to the application and need only
   outlive the widget call, hence the copy. The bracket is reserved first and
   the label takes what is left: a truncated label still reads, a truncated
   value would just mislead.
 *
 * Writes the title and its terminator, leaving the keystrokes to start at
 * edit_head.
 */
void set_edit_title(atc_menu_ctx_t *c, const char *label, const char *value)
{
    char  *t = edit_area(c);
    size_t max = title_max(c);
    size_t vlen = (value != NULL) ? strlen(value) : 0u;
    size_t n = 0u;
    size_t room;
    size_t i;

    if (vlen > VALW)
        vlen = VALW;
    /* " [" + value + "]" is what the label may not eat into. */
    room = (vlen > 0u && max > vlen + 3u) ? max - (vlen + 3u) : max;

    if (label != NULL) {
        while (label[n] != '\0' && n < room)
            n++;
        memcpy(t, label, n);
    }
    c->edit_vpos = 0u; /* 0 means the title carries no value to pick out */
    if (vlen > 0u && n + vlen + 3u <= max) {
        if (n > 0u)
            t[n++] = ' ';
        t[n++] = '[';
        c->edit_vpos = (unsigned char)n;
        for (i = 0u; i < vlen; ++i)
            t[n++] = value[i];
        t[n++] = ']';
    }
    t[n] = '\0';
    c->edit_head = (unsigned char)(n + 1u); /* keystrokes start past the NUL */
    t[c->edit_head] = '\0';
}

/*---------------------------------------------------------------------------
 * Opening and closing
 *-------------------------------------------------------------------------*/

void begin_edit(atc_menu_ctx_t *c, unsigned index, unsigned base,
                unsigned decimals, const char *label, const char *value)
{
    c->flags = (uint16_t)((c->flags & ~(F_COMMIT | F_NEG | F_OVF | F_EDIT_KIND |
                                        F_DELIVERED)) | F_EDIT);
    c->edit_item = (unsigned char)index;
    c->edit_base = (unsigned char)base;
    c->edit_dec = (unsigned char)decimals;
    c->edit_frac = NO_FRAC;
    c->edit_len = 0u; /* digits typed, so an untouched editor prompts empty */
    c->acc = 0;
    c->msg = NULL;
    set_edit_title(c, label, value);
}

/* Closing the editor also retires whatever it had to say: a refusal explains an
   entry that is no longer on screen. */
void end_edit(atc_menu_ctx_t *c)
{
    c->flags &= (uint16_t)~(F_EDIT | F_COMMIT | F_NEG | F_OVF | F_EDIT_KIND |
                            F_DELIVERED);
    c->msg = NULL;
}

void begin_edit_text(atc_menu_ctx_t *c, unsigned index, const char *label,
                     const char *value)
{
    c->flags = (uint16_t)((c->flags & ~(F_COMMIT | F_NEG | F_OVF | F_CHOICE |
                                        F_DELIVERED)) | F_EDIT | F_TEXT);
    c->edit_item = (unsigned char)index;
    c->edit_len = 0u;
    c->msg = NULL;
    set_edit_title(c, label, value);
}

/* acc is the candidate index and edit_base the number of choices, which is what
   lets key_choice() wrap on its own. It opens on the option after the committed
   one, so the keystroke that opened the editor is never a wasted step. */
void begin_edit_choice(atc_menu_ctx_t *c, unsigned index, unsigned first,
                       unsigned count, const char *label, const char *value)
{
    c->flags = (uint16_t)((c->flags & ~(F_COMMIT | F_NEG | F_OVF | F_TEXT |
                                        F_DELIVERED)) | F_EDIT | F_CHOICE);
    c->edit_item = (unsigned char)index;
    c->edit_base = (unsigned char)((count > 255u) ? 255u : count);
    c->edit_len = 0u;
    c->acc = first;
    c->msg = NULL;
    set_edit_title(c, label, value);
}

/*---------------------------------------------------------------------------
 * Handing the value over, and getting it back
 *-------------------------------------------------------------------------*/

bool commit_ready(atc_menu_ctx_t *c, unsigned index)
{
    if (index == 0u)
        return false; /* off this page, so it cannot be what is being edited */
    if ((c->flags & (F_EDIT | F_COMMIT)) != (F_EDIT | F_COMMIT))
        return false;
    return c->edit_item == index;
}

/* The value is on its way to the application, which may still refuse it. The
   editor leaves the screen but its state stays whole until frame_end. */
void deliver(atc_menu_ctx_t *c)
{
    c->flags = (uint16_t)((c->flags & ~(F_EDIT | F_COMMIT)) | F_DELIVERED);
    c->msg = NULL; /* a refusal this entry has answered; reject() sets its own */
}

/* Refused by the library itself — out of the row's range, or too long for the
   caller's buffer. Same outcome as atc_menu_reject(), without the round trip:
   the editor stays open over what was typed. */
bool refuse(atc_menu_ctx_t *c, const char *msg)
{
    c->flags &= (uint16_t)~F_COMMIT;
    c->msg = msg;
    return false;
}

void atc_menu_reject(atc_menu_ctx_t *c, const char *msg)
{
    if (c == NULL || (c->flags & F_DELIVERED) == 0u)
        return;
    c->flags = (uint16_t)((c->flags & ~F_DELIVERED) | F_EDIT);
    c->msg = msg;
}
