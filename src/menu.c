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

static const atc_menu_item_t *g_items;
static size_t                 g_count;
static const atc_menu_port_t *g_port;
static const atc_menu_info_t *g_info;

static bool   g_cmd_mode;
static char   g_cmd_buf[MENU_CMD_BUF];
static size_t g_cmd_len;

static bool   g_status_dirty;

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
    for (int i = 0; i < MENU_HDR_INNER + 2; i++) atc_menu_printf(SYM_BOX_H);
    atc_menu_printf("%s" ANSI_RESET ANSI_EOL "\n", right);
}

static void print_info_row(const char *l, const char *r,
                           const char *l_color, const char *r_color) {
    int lw  = l ? (int)strlen(l) : 0;
    int rw  = r ? (int)strlen(r) : 0;
    int gap = MENU_HDR_INNER - lw - rw;
    if (gap < 1) gap = 1;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET " %s%s" ANSI_RESET,
                    l_color ? l_color : "", l ? l : "");
    atc_menu_printf("%*s", gap, "");
    atc_menu_printf("%s%s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET
                    ANSI_EOL "\n",
                    r_color ? r_color : "", r ? r : "");
}

static void print_header(void) {
    const char *project = (g_info && g_info->project) ? g_info->project
                                                      : "atc menu";
    const char *version = (g_info && g_info->version) ? g_info->version : NULL;
    const char *author  = (g_info && g_info->author)  ? g_info->author  : NULL;
    const char *build   = (g_info && g_info->build)   ? g_info->build   : NULL;

    print_box_edge(SYM_BOX_TL, SYM_BOX_TR);
    print_info_row(project, version, ANSI_BOLD ANSI_FG_KEY, ANSI_DIM);
    if (author || build)
        print_info_row(author, build, ANSI_DIM, ANSI_DIM);
    print_box_edge(SYM_BOX_LJ, SYM_BOX_RJ);
}

static void print_footer(void) {
    atc_menu_printf("\n" ANSI_DIM "[r] refresh  [:] cmd" ANSI_RESET
                    ANSI_EOL "\n");
}

static void print_group(int zebra_idx, const atc_menu_item_t *it) {
    const char *bg    = (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
    const char *label = it->label ? it->label : "";
    int         lw    = (int)strlen(label);
    /* Same 5-char prefix as rows (lead + key + 2 spaces) so labels align. */
    int         pad   = MENU_HDR_INNER - 3 - lw;
    if (pad < 0) pad = 0;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET "%s ", bg);
    atc_menu_printf(ANSI_FG_KEY "%c" ANSI_RESET "%s  ",
                    it->key ? it->key : ' ', bg);
    atc_menu_printf(ANSI_BOLD "%s" ANSI_RESET "%s", label, bg);
    atc_menu_printf("%*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET
                    ANSI_EOL "\n", pad, "");
}

static void print_row(int zebra_idx, const atc_menu_item_t *it) {
    const char  *bg                 = (zebra_idx & 1) ? ANSI_BG_ZEBRA : "";
    char         val[MENU_BUF_SIZE] = {0};
    atc_status_t st                 = ATC_ST_NONE;
    if (it->read) it->read(val, MENU_BUF_SIZE, &st);

    const status_disp_t *sd = status_disp(st);
    int sw  = (int)strlen(sd->text);
    int pad = MENU_HDR_INNER - (1 + 2 + MENU_LABEL_COL + MENU_VALUE_COL
                                + 1 + MENU_UNIT_COL + 2 + sw);
    if (pad < 0) pad = 0;

    atc_menu_printf(ANSI_DIM SYM_BOX_V ANSI_RESET "%s ", bg);
    atc_menu_printf(ANSI_FG_KEY "%c" ANSI_RESET "%s  ",
                    it->key ? it->key : ' ', bg);
    atc_menu_printf("%-*s", MENU_LABEL_COL, it->label ? it->label : "");
    atc_menu_printf(ANSI_FG_VAL "%*s" ANSI_RESET "%s",
                    MENU_VALUE_COL, val, bg);
    atc_menu_printf(" " ANSI_DIM "%-*s" ANSI_RESET "%s",
                    MENU_UNIT_COL, it->unit ? it->unit : "", bg);
    atc_menu_printf("  %s%s" ANSI_RESET "%s", sd->color, sd->text, bg);
    atc_menu_printf("%*s" ANSI_RESET " " ANSI_DIM SYM_BOX_V ANSI_RESET
                    ANSI_EOL "\n", pad, "");
}

/* Header is 3 lines (top, title, separator) or 4 if author/build shown. */
static int header_lines(void) {
    if (g_info && (g_info->author || g_info->build)) return 4;
    return 3;
}

void atc_menu_render(void) {
    if (!g_items || !g_port) return;

    atc_menu_printf(ANSI_HOME);
    print_header();

    int zebra = 0;
    for (size_t i = 0; i < g_count; i++) {
        const atc_menu_item_t *it = &g_items[i];
        if (it->type == ATC_ROW_GROUP)
            print_group(zebra++, it);
        else
            print_row(zebra++, it);
    }
    print_box_edge(SYM_BOX_BL, SYM_BOX_BR);
    print_footer();
    atc_menu_printf(ANSI_CLR_BELOW);
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

    const atc_menu_item_t *it = &g_items[index];
    if (it->type == ATC_ROW_GROUP)
        print_group((int)index, it);
    else
        print_row((int)index, it);

    park_cursor_below();
}

/* Bulk-refresh: group label + all rows until the next group. */
static void render_group_span(size_t start) {
    size_t end = start + 1;
    while (end < g_count && g_items[end].type != ATC_ROW_GROUP) end++;

    int y = header_lines() + 1 + (int)start;
    atc_menu_printf(ANSI_GOTO_FMT ANSI_EOL, y);
    for (size_t i = start; i < end; i++) {
        const atc_menu_item_t *it = &g_items[i];
        if (it->type == ATC_ROW_GROUP) print_group((int)i, it);
        else                           print_row((int)i, it);
    }
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
            atc_menu_printf("\b \b");  /* erase one echoed char */
        }
    } else if (g_cmd_len < sizeof g_cmd_buf - 1) {
        g_cmd_buf[g_cmd_len++] = k;
        g_port->write(&k, 1);          /* echo */
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void atc_menu_set_info(const atc_menu_info_t *info) {
    g_info = info;
}

void atc_menu_init(const atc_menu_item_t *items, size_t count,
                   const atc_menu_port_t *port) {
    g_items    = items;
    g_count    = count;
    g_port     = port;
    g_cmd_mode = false;
    g_cmd_len  = 0;

    if (!items || !port) return;

    for (size_t i = 0; i < count; i++) {
        const atc_menu_item_t *a = &items[i];

        if (a->type == ATC_ROW_VALUE && !a->read)
            atc_menu_printf("WARN: ATC_ROW_VALUE '%c' missing read\n", a->key);
        if (a->type == ATC_ROW_STATE && (!a->read || !a->action))
            atc_menu_printf("WARN: ATC_ROW_STATE '%c' missing read/action\n", a->key);
        if (a->type == ATC_ROW_ACTION && !a->action)
            atc_menu_printf("WARN: ATC_ROW_ACTION '%c' missing action\n", a->key);

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
        atc_menu_printf("\n" SYM_CMD_PROMPT);
        return;
    }
    if (k == 'r') {
        atc_menu_render();
        return;
    }

    g_status_dirty = false;
    for (size_t i = 0; i < g_count; i++) {
        if (g_items[i].key != k) continue;
        if (g_items[i].action) g_items[i].action();
        if (g_items[i].type == ATC_ROW_GROUP) render_group_span(i);
        else                                  render_row(i);
        return;
    }
}

void atc_menu_status(const char *msg) {
    atc_menu_printf("\n" ANSI_DIM SYM_PROMPT "%s" ANSI_EOL ANSI_RESET "\n",
                    msg ? msg : "");
    g_status_dirty = true;
}
