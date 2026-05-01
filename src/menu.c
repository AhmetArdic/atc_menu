/*
 * Copyright (C) 2026 Ahmet Talha ARDIC
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file menu.c
 * @brief UART debug menu framework — implementation.
 */

#include "atc_menu/atc_menu.h"

#include "ansi.h"
#include "layout.h"
#include "symbols.h"

#include <stdio.h>
#include <string.h>

enum { KEY_BS = 8, KEY_ESC = 27, KEY_DEL = 127 };

#ifndef ATC_MENU_STACK_DEPTH
#define ATC_MENU_STACK_DEPTH 4
#endif

static const atc_menu_item_t *g_items;
static size_t                 g_count;
static const atc_menu_port_t *g_port;
static const atc_menu_info_t *g_info;

static bool   g_cmd_mode;
static char   g_cmd_buf[MENU_CMD_BUF];
static size_t g_cmd_len;

static bool   g_status_dirty;

typedef struct {
    const atc_menu_item_t *items;
    size_t                 count;
    const char            *label;
} menu_frame_t;

static menu_frame_t g_stack[ATC_MENU_STACK_DEPTH];
static size_t       g_stack_depth;
static const char  *g_active_label;  /* current menu's name; NULL at root */

/* ------------------------------------------------------------------ */
/* Status decoding                                                     */
/* ------------------------------------------------------------------ */

typedef struct { const char *color, *text; } status_disp_t;

static const status_disp_t STATUS[] = {
    [ATC_ST_NONE] = { "",            ""           },
    [ATC_ST_OK]   = { ANSI_FG_OK,    SYM_ST_OK    },
    [ATC_ST_WARN] = { ANSI_FG_WARN,  SYM_ST_WARN  },
    [ATC_ST_ERR]  = { ANSI_FG_ERR,   SYM_ST_ERR   },
    [ATC_ST_ON]   = { ANSI_FG_OK,    SYM_ST_ON    },
    [ATC_ST_OFF]  = { ANSI_DIM,      SYM_ST_OFF   },
};

static const status_disp_t *status_disp(atc_status_t st) {
    static const status_disp_t empty = { "", "" };
    if ((unsigned)st >= sizeof STATUS / sizeof STATUS[0]) return &empty;
    return &STATUS[st];
}

/* ------------------------------------------------------------------ */
/* Output                                                              */
/* ------------------------------------------------------------------ */

int atc_menu_printf(const char *fmt, ...) {
    if (!g_port || !g_port->write) return 0;

    char    b[MENU_PRINT_BUF];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);

    if (n <= 0) return n;
    size_t out = (size_t)n < sizeof b ? (size_t)n : sizeof b - 1;
    g_port->write(b, out);
    return (int)out;
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */

static void print_box_edge(const char *left, const char *right) {
    atc_menu_printf(ANSI_DIM "%s", left);
    for (int i = 0; i < MENU_HDR_INNER + 2; i++) atc_menu_printf("%s", SYM_BOX_H);
    atc_menu_printf("%s" ANSI_RESET ANSI_EOL "\n", right);
}

/* Top edge with an inline title (and optional version on the right):
 *   +- TITLE ----------------------- VERSION -+
 * Falls back gracefully if either piece doesn't fit. */
static void print_top_edge(const char *title, const char *version) {
    const int total = MENU_HDR_INNER + 2;
    int       used  = 0;

    atc_menu_printf("%s", ANSI_DIM SYM_BOX_TL SYM_BOX_H);
    used = 1;

    int tl = title   ? (int)strlen(title)   : 0;
    int vl = version ? (int)strlen(version) : 0;

    /* Reserve space: leading "- " for title (already 1 used + 1 space),
     * trailing " VERSION -" if version present. */
    int reserve_right = vl ? (vl + 3) : 0;  /* " " + version + " -" */

    if (title && used + tl + 2 + reserve_right <= total) {
        atc_menu_printf(" " ANSI_RESET ANSI_BOLD ANSI_FG_KEY "%s" ANSI_RESET ANSI_DIM " ", title);
        used += tl + 2;
    }

    /* Trailing dashes up to the version field (if any), or all the way. */
    int dashes_end = total - reserve_right;
    for (; used < dashes_end; used++) atc_menu_printf("%s", SYM_BOX_H);

    if (version && used + reserve_right <= total) {
        atc_menu_printf(" " ANSI_RESET ANSI_FG_VAL "%s" ANSI_RESET ANSI_DIM " ", version);
        used += vl + 2;
    }

    for (; used < total; used++) atc_menu_printf("%s", SYM_BOX_H);
    atc_menu_printf("%s", SYM_BOX_TR ANSI_RESET ANSI_EOL "\n");
}

static void print_info_row(const char *l, const char *r, const char *l_color, const char *r_color) {
    int lw = l ? (int)strlen(l) : 0;
    int rw = r ? (int)strlen(r) : 0;
    if (lw > MENU_HDR_INNER - 1)      lw = MENU_HDR_INNER - 1;
    if (rw > MENU_HDR_INNER - lw - 1) rw = MENU_HDR_INNER - lw - 1;
    int gap = MENU_HDR_INNER - lw - rw;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET " %s%.*s" ANSI_RESET, l_color ? l_color : "", lw, l ? l : "");
    atc_menu_printf("%*s", gap, "");
    atc_menu_printf("%s%.*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\n", r_color ? r_color : "", rw, r ? r : "");
}

static void show_path(void) {
    char   b[MENU_PRINT_BUF];
    size_t n = 0;
    int    w = snprintf(b, sizeof b, "%s", SYM_CRUMB_ROOT);
    if (w > 0) n = (size_t)w < sizeof b ? (size_t)w : sizeof b - 1;

    for (size_t i = 0; i <= g_stack_depth && n + 1 < sizeof b; i++) {
        const char *lbl = (i < g_stack_depth) ? g_stack[i].label : g_active_label;
        if (!lbl) continue;
        w = snprintf(b + n, sizeof b - n, "%s%s", SYM_CRUMB, lbl);
        if (w > 0) n += (size_t)w < sizeof b - n ? (size_t)w : sizeof b - n - 1;
    }

    atc_menu_printf("\n" ANSI_DIM "%.*s" ANSI_EOL ANSI_RESET "\n", (int)n, b);
    g_status_dirty = true;
}

static void print_header(void) {
    const char *project = (g_info && g_info->project) ? g_info->project
                                                      : "atc menu";
    const char *version = (g_info && g_info->version) ? g_info->version : NULL;
    const char *author  = (g_info && g_info->author)  ? g_info->author  : NULL;
    const char *build   = (g_info && g_info->build)   ? g_info->build   : NULL;

    print_top_edge(project, version);
    if (author || build)
        print_info_row(author, build, ANSI_DIM, ANSI_DIM);
    print_box_edge(SYM_BOX_LJ, SYM_BOX_RJ);
}

static void print_footer(void) {
    if (g_stack_depth > 0)
        atc_menu_printf("\n" ANSI_DIM "[r] refresh  [b] back  [?] path  [:] cmd" ANSI_RESET ANSI_EOL "\n");
    else
        atc_menu_printf("\n" ANSI_DIM "[r] refresh  [:] cmd" ANSI_RESET ANSI_EOL "\n");
}

static void print_group(int zebra_idx, const atc_menu_item_t *it) {
    const char *bg     = (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
    const char *label  = it->label ? it->label : "";
    /* Same 5-char prefix as rows (lead + key + 2 spaces) so labels align. */
    int         lw_max = MENU_HDR_INNER - 3;
    int         lw     = (int)strlen(label);
    if (lw > lw_max) lw = lw_max;
    int         pad    = lw_max - lw;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET "%s ", bg);
    atc_menu_printf(ANSI_FG_KEY "%c" ANSI_RESET "%s  ", it->key ? it->key : ' ', bg);
    atc_menu_printf(ANSI_BOLD "%.*s" ANSI_RESET "%s", lw, label, bg);
    atc_menu_printf("%*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\n", pad, "");
}

static void print_row(int zebra_idx, const atc_menu_item_t *it) {
    const char  *bg                 = (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, MENU_BUF_SIZE, &st);

    const status_disp_t *sd = status_disp(st);
    int sw  = (int)strlen(sd->text);
    int pad = MENU_HDR_INNER - (1 + 2 + MENU_LABEL_COL + MENU_VALUE_COL + 1 + MENU_UNIT_COL + 2 + sw);
    if (pad < 0) pad = 0;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET "%s ", bg);
    atc_menu_printf(ANSI_FG_KEY "%c" ANSI_RESET "%s  ", it->key ? it->key : ' ', bg);
    atc_menu_printf("%-*.*s", MENU_LABEL_COL, MENU_LABEL_COL, it->label ? it->label : "");
    atc_menu_printf(ANSI_FG_VAL "%*.*s" ANSI_RESET "%s", MENU_VALUE_COL, MENU_VALUE_COL, val, bg);
    atc_menu_printf(" " ANSI_DIM "%-*.*s" ANSI_RESET "%s", MENU_UNIT_COL, MENU_UNIT_COL, it->unit ? it->unit : "", bg);
    atc_menu_printf("  %s%s" ANSI_RESET "%s", sd->color, sd->text, bg);
    atc_menu_printf("%*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\n", pad, "");
}

static void print_submenu(int zebra_idx, const atc_menu_item_t *it) {
    const char *bg          = (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
    const char *label       = it->label ? it->label : "";
    int         label_w_max = MENU_LABEL_COL - 2;  /* "> " takes 2 chars */
    int         pad         = MENU_HDR_INNER - (1 + 2 + 2 + label_w_max);
    if (pad < 0) pad = 0;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET "%s ", bg);
    atc_menu_printf(ANSI_FG_KEY "%c" ANSI_RESET "%s  ", it->key ? it->key : ' ', bg);
    atc_menu_printf(ANSI_FG_KEY SYM_SUBMENU ANSI_RESET "%s", bg);
    atc_menu_printf("%-*.*s", label_w_max, label_w_max, label);
    atc_menu_printf("%*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET ANSI_EOL "\n", pad, "");
}

static int header_lines(void) {
    int n = 2;
    if (g_info && (g_info->author || g_info->build)) n++;
    return n;
}

static void print_item(int zebra, const atc_menu_item_t *it) {
    if (it->type == ATC_ROW_GROUP)        print_group(zebra, it);
    else if (it->type == ATC_ROW_SUBMENU) print_submenu(zebra, it);
    else                                  print_row(zebra, it);
}

void atc_menu_render(void) {
    if (!g_items || !g_port) return;

    atc_menu_printf("%s", ANSI_HOME);
    print_header();

    for (size_t i = 0; i < g_count; i++) print_item((int)i, &g_items[i]);

    print_box_edge(SYM_BOX_BL, SYM_BOX_BR);
    /* Two wipes: clear stale footer/status before drawing the new (possibly
     * shorter) footer, then clear anything below it. */
    atc_menu_printf("%s", ANSI_CLR_BELOW);
    print_footer();
    atc_menu_printf("%s", ANSI_CLR_BELOW);
    g_status_dirty = false;
}

/* Wipe stale lines below the menu unless the current pass printed a
 * status — then preserve it until the next key. */
static void park_cursor_below(void) {
    int below = header_lines() + (int)g_count + 4;
    if (g_status_dirty) atc_menu_printf(ANSI_GOTO_FMT, below);
    else                atc_menu_printf(ANSI_GOTO_FMT ANSI_CLR_BELOW, below);
}

static void render_row(size_t index) {
    int y = header_lines() + 1 + (int)index;
    atc_menu_printf(ANSI_GOTO_FMT ANSI_EOL, y);
    print_item((int)index, &g_items[index]);
    park_cursor_below();
}

/* Bulk-refresh: group label + all rows until the next group. */
static void render_group_span(size_t start) {
    size_t end = start + 1;
    while (end < g_count && g_items[end].type != ATC_ROW_GROUP) end++;

    int y = header_lines() + 1 + (int)start;
    atc_menu_printf(ANSI_GOTO_FMT ANSI_EOL, y);
    for (size_t i = start; i < end; i++) print_item((int)i, &g_items[i]);
    park_cursor_below();
}

/* ------------------------------------------------------------------ */
/* Command mode                                                        */
/* ------------------------------------------------------------------ */

static void cmd_input(char k) {
    if (k == '\r' || k == '\n') {
        g_cmd_buf[g_cmd_len] = '\0';
        g_cmd_mode           = false;
        g_port->cmd(g_cmd_buf);
        g_cmd_len = 0;
        atc_menu_render();
    } else if (k == KEY_ESC) {
        g_cmd_mode = false;
        g_cmd_len  = 0;
        park_cursor_below();
    } else if (k == KEY_BS || k == KEY_DEL) {
        if (g_cmd_len) {
            g_cmd_len--;
            atc_menu_printf("%s", "\b \b");  /* erase one echoed char */
        }
    } else if (g_cmd_len < sizeof g_cmd_buf - 1) {
        g_cmd_buf[g_cmd_len++] = k;
        g_port->write(&k, 1);          /* echo */
    }
}

/* ------------------------------------------------------------------ */
/* Navigation                                                          */
/* ------------------------------------------------------------------ */

static void push_frame(const atc_menu_item_t *opener) {
    if (!opener->submenu || opener->submenu_count == 0) {
        atc_menu_printf("WARN: SUBMENU '%c' has no submenu table\n", opener->key);
        return;
    }
    if (g_stack_depth >= ATC_MENU_STACK_DEPTH) {
        atc_menu_printf("WARN: nav stack full (depth %d)\n", ATC_MENU_STACK_DEPTH);
        return;
    }
    g_stack[g_stack_depth].items = g_items;
    g_stack[g_stack_depth].count = g_count;
    g_stack[g_stack_depth].label = g_active_label;
    g_stack_depth++;
    g_items        = opener->submenu;
    g_count        = opener->submenu_count;
    g_active_label = opener->label;
}

static void pop_frame(void) {
    if (g_stack_depth == 0) return;
    g_stack_depth--;
    g_items        = g_stack[g_stack_depth].items;
    g_count        = g_stack[g_stack_depth].count;
    g_active_label = g_stack[g_stack_depth].label;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void atc_menu_set_info(const atc_menu_info_t *info) {
    g_info = info;
}

void atc_menu_init(const atc_menu_item_t *items, size_t count,
                   const atc_menu_port_t *port) {
    g_items        = items;
    g_count        = count;
    g_port         = port;
    g_cmd_mode     = false;
    g_cmd_len      = 0;
    g_stack_depth  = 0;
    g_active_label = NULL;

    if (!items || !port) return;

    for (size_t i = 0; i < count; i++) {
        const atc_menu_item_t *a = &items[i];

        if (a->type == ATC_ROW_VALUE && !a->read)
            atc_menu_printf("WARN: ATC_ROW_VALUE '%c' missing read\n", a->key);
        if (a->type == ATC_ROW_STATE && (!a->read || !a->action))
            atc_menu_printf("WARN: ATC_ROW_STATE '%c' missing read/action\n", a->key);
        if (a->type == ATC_ROW_ACTION && !a->action)
            atc_menu_printf("WARN: ATC_ROW_ACTION '%c' missing action\n", a->key);
        if (a->type == ATC_ROW_SUBMENU && (!a->submenu || a->submenu_count == 0))
            atc_menu_printf("WARN: ATC_ROW_SUBMENU '%c' missing submenu\n", a->key);

        /* GROUP fills the box; SUBMENU loses 2 cols to the "> " marker. */
        if (a->label) {
            size_t lmax = (a->type == ATC_ROW_GROUP)   ? MENU_HDR_INNER - 3
                        : (a->type == ATC_ROW_SUBMENU) ? MENU_LABEL_COL - 2
                                                       : MENU_LABEL_COL;
            if (strlen(a->label) > lmax)
                atc_menu_printf("WARN: label '%s' exceeds %zu cols\n", a->label, lmax);
        }
        if ((a->type == ATC_ROW_VALUE || a->type == ATC_ROW_STATE)
            && a->unit && strlen(a->unit) > MENU_UNIT_COL)
            atc_menu_printf("WARN: unit '%s' exceeds %d cols\n", a->unit, MENU_UNIT_COL);

        if (a->key == 'b')
            atc_menu_printf("WARN: key 'b' collides with built-in back\n");
        if (a->key == '?')
            atc_menu_printf("WARN: key '?' collides with built-in path\n");

        if (!a->key) continue;
        for (size_t j = i + 1; j < count; j++) {
            if (a->key == items[j].key)
                atc_menu_printf("WARN: dup key '%c'\n", a->key);
        }
    }
}

void atc_menu_handle_key(char k) {
    if (!g_items || !g_port) return;

    if (g_cmd_mode) {
        cmd_input(k);
        return;
    }

    if (k == ':' && g_port->cmd) {
        g_cmd_mode = true;
        g_cmd_len  = 0;
        atc_menu_printf("%s", "\n" SYM_CMD_PROMPT);
        return;
    }
    if (k == 'r') {
        atc_menu_render();
        return;
    }
    if (k == 'b' && g_stack_depth > 0) {
        pop_frame();
        atc_menu_render();
        return;
    }
    if (k == '?' && g_stack_depth > 0) {
        show_path();
        return;
    }

    g_status_dirty = false;
    for (size_t i = 0; i < g_count; i++) {
        if (g_items[i].key != k) continue;
        if (g_items[i].type == ATC_ROW_SUBMENU) {
            push_frame(&g_items[i]);
            atc_menu_render();
            return;
        }
        if (g_items[i].action) g_items[i].action();
        if (g_items[i].type == ATC_ROW_GROUP) render_group_span(i);
        else                                  render_row(i);
        return;
    }
}

void atc_menu_status(const char *msg) {
    atc_menu_printf("\n" ANSI_DIM SYM_PROMPT "%s" ANSI_EOL ANSI_RESET "\n", msg ? msg : "");
    g_status_dirty = true;
}
