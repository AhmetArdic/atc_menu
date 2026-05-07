/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "render/row.h"

#include <stdarg.h>
#include <stdio.h>

/* ---------------------------------------------------------------- helpers */

static int utf8_cols(const char *s) {
    int cols = 0;
    if (!s) return 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        int adv;
        if      ((c & 0x80) == 0x00) adv = 1;
        else if ((c & 0xE0) == 0xC0) adv = 2;
        else if ((c & 0xF0) == 0xE0) adv = 3;
        else if ((c & 0xF8) == 0xF0) adv = 4;
        else                         adv = 1;
        cols++;
        s += adv;
    }
    return cols;
}

static const char *zebra_bg(int zebra_idx) {
    return (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
}

/* ---------------------------------------------------------------- Layer 1 */

void row_buf_reset(row_buf_t *b) {
    b->len = 0;
}

void row_buf_flush(row_buf_t *b) {
    const atc_menu_port_t *p = menu_port();
    if (p && p->write && b->len > 0) p->write(b->buf, (size_t)b->len);
    b->len = 0;
}

static void row_buf_vappend(row_buf_t *b, const char *fmt, va_list ap) {
    int avail = (int)sizeof b->buf - b->len;
    if (avail <= 1) return;
    int n = vsnprintf(b->buf + b->len, (size_t)avail, fmt, ap);
    if (n < 0)      return;
    if (n >= avail) n = avail - 1;
    b->len += n;
}

void row_buf_printf(row_buf_t *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    row_buf_vappend(b, fmt, ap);
    va_end(ap);
}

/* ---------------------------------------------------------------- Layer 3 */

static void emit_region(row_t *r,
                        const region_def_t     *def,
                        const region_content_t *c) {
    const char *text  = c->text  ? c->text  : "";
    const char *style = (c->style && *c->style) ? c->style : def->default_style;
    int         cols  = utf8_cols(text);
    int         pad   = def->width - cols;
    bool        styled = style && *style;

    if (styled) row_buf_printf(&r->out, "%s", style);

    if (cols > def->width) {
        /* Truncate by byte — matches legacy row_cell behaviour. */
        row_buf_printf(&r->out, "%-*.*s", def->width, def->width, text);
    } else if (def->align == REGION_ALIGN_RIGHT) {
        if (pad > 0) row_buf_printf(&r->out, "%*s", pad, "");
        row_buf_printf(&r->out, "%s", text);
    } else {
        row_buf_printf(&r->out, "%s", text);
        if (pad > 0) row_buf_printf(&r->out, "%*s", pad, "");
    }

    if (styled) row_buf_printf(&r->out, ANSI_RESET "%s", r->bg);
}

void row_begin(row_t *r, const row_layout_t *layout, int zebra_idx) {
    r->layout    = layout;
    r->zebra_idx = zebra_idx;
    r->bg        = zebra_bg(zebra_idx);
    row_buf_reset(&r->out);

    size_t n = layout ? layout->count : 0;
    if (n > ATC_ROW_MAX_REGIONS) n = ATC_ROW_MAX_REGIONS;
    for (size_t i = 0; i < n; i++) {
        r->content[i].style = NULL;
        r->content[i].text  = NULL;
    }
}

void row_set(row_t *r, size_t region_idx,
             const char *style, const char *text) {
    if (!r->layout || region_idx >= r->layout->count) return;
    if (region_idx >= ATC_ROW_MAX_REGIONS)             return;
    r->content[region_idx].style = style;
    r->content[region_idx].text  = text;
}

void row_end(row_t *r) {
    if (!r->layout) return;

    row_buf_printf(&r->out, ANSI_DIM SYM_BOX_V ANSI_RESET "%s", r->bg);

    for (size_t i = 0; i < r->layout->count; i++) {
        emit_region(r, &r->layout->regions[i], &r->content[i]);
        if (i + 1 < r->layout->count && r->layout->regions[i].gap_after > 0)
            row_buf_printf(&r->out, "%*s",
                           r->layout->regions[i].gap_after, "");
    }

    row_buf_printf(&r->out, ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\r\n");
    row_buf_flush(&r->out);
}

/* ---------------------------------------------------------------- layouts */

#define GAP MENU_FIELD_GAP_W

static const region_def_t scalar_regions[] = {
    { MENU_KEY_COL,    REGION_ALIGN_LEFT,  ANSI_FG_KEY, GAP },
    { MENU_LABEL_COL,  REGION_ALIGN_LEFT,  NULL,        GAP },
    { MENU_VALUE_COL,  REGION_ALIGN_RIGHT, ANSI_FG_VAL, GAP },
    { MENU_UNIT_COL,   REGION_ALIGN_LEFT,  ANSI_DIM,    GAP },
    { MENU_STATUS_COL, REGION_ALIGN_LEFT,  NULL,        0   },
};
const row_layout_t ROW_LAYOUT_SCALAR = {
    .regions = scalar_regions,
    .count   = sizeof scalar_regions / sizeof scalar_regions[0],
};

static const region_def_t group_regions[] = {
    { MENU_KEY_COL,        REGION_ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_GROUP_LABEL_W,  REGION_ALIGN_LEFT, NULL,        0   },
};
const row_layout_t ROW_LAYOUT_GROUP = {
    .regions = group_regions,
    .count   = sizeof group_regions / sizeof group_regions[0],
};

static const region_def_t submenu_regions[] = {
    { MENU_KEY_COL,           REGION_ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_SUBMENU_PROMPT_W,  REGION_ALIGN_LEFT, ANSI_FG_KEY, 0   },
    { MENU_SUBMENU_LABEL_W,   REGION_ALIGN_LEFT, NULL,        0   },
};
const row_layout_t ROW_LAYOUT_SUBMENU = {
    .regions = submenu_regions,
    .count   = sizeof submenu_regions / sizeof submenu_regions[0],
};

static const region_def_t input_edit_regions[] = {
    { MENU_KEY_COL,       REGION_ALIGN_LEFT, ANSI_FG_KEY, GAP },
    { MENU_LABEL_COL,     REGION_ALIGN_LEFT, NULL,        GAP },
    { MENU_INPUT_EDIT_W,  REGION_ALIGN_LEFT, ANSI_FG_VAL, 0   },
};
const row_layout_t ROW_LAYOUT_INPUT_EDIT = {
    .regions = input_edit_regions,
    .count   = sizeof input_edit_regions / sizeof input_edit_regions[0],
};

static const region_def_t note_regions[] = {
    { MENU_NOTE_W, REGION_ALIGN_LEFT, ANSI_DIM, 0 },
};
const row_layout_t ROW_LAYOUT_NOTE = {
    .regions = note_regions,
    .count   = sizeof note_regions / sizeof note_regions[0],
};

#undef GAP

/* ---------------------------------------------------------------- status */

static const status_disp_t STATUS[] = {
    [ATC_ST_NONE] = { "",            ""           },
    [ATC_ST_OK]   = { ANSI_FG_OK,    SYM_ST_OK    },
    [ATC_ST_WARN] = { ANSI_FG_WARN,  SYM_ST_WARN  },
    [ATC_ST_ERR]  = { ANSI_FG_ERR,   SYM_ST_ERR   },
    [ATC_ST_ON]   = { ANSI_FG_OK,    SYM_ST_ON    },
    [ATC_ST_OFF]  = { ANSI_DIM,      SYM_ST_OFF   },
};

const status_disp_t *status_disp(atc_status_t st) {
    static const status_disp_t empty = { "", "" };
    if ((unsigned)st >= sizeof STATUS / sizeof STATUS[0]) return &empty;
    return &STATUS[st];
}
