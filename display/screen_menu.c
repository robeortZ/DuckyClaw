/**
 * @file screen_menu.c
 * @brief Vertical menu with same arc-scroll effect as todo list (lv_example_scroll_6 style)
 *
 * Each item shows an LV_SYMBOL icon (FontAwesome glyphs in LV_FONT_DEFAULT / Montserrat)
 * and label text in the puhui font from ai_ui_get_text_font().
 */
#include "screen_menu.h"
#include "screen_manager.h"
#include "screen_todo_list.h"
#include "screen_cron_list.h"
#include "screen_music.h"
#include "screen_settings.h"
#include "nav_gesture.h"
#include "ui_layout.h"
#include "ui_scroll_arc.h"
#include "ducky_ui_strings.h"
#include "ai_ui_icon_font.h"

#include "lvgl.h"

#include <stddef.h>

static lv_obj_t *ui_scr;
static lv_obj_t *ui_list;
static lv_obj_t *s_menu_title;

static void screen_menu_init(void);
static void screen_menu_deinit(void);

Screen_t screen_menu = {
    .init = screen_menu_init,
    .deinit = screen_menu_deinit,
    .screen_obj = &ui_scr,
    .name = "Menu",
};

typedef struct {
    ducky_ui_string_id_e text_id;
    Screen_t *           scr;
    const char *         symbol; /**< LV_SYMBOL_* string; must use LV_FONT_DEFAULT on label */
} menu_item_desc_t;

static const menu_item_desc_t s_items[] = {
    { DUCKY_UI_STR_MENU_TODO, &screen_todo, LV_SYMBOL_LIST },
    { DUCKY_UI_STR_MENU_CRON, &screen_cron, LV_SYMBOL_BELL },
    { DUCKY_UI_STR_MENU_MUSIC, &screen_music, LV_SYMBOL_AUDIO },
    { DUCKY_UI_STR_MENU_SETTINGS, &screen_settings, LV_SYMBOL_SETTINGS },
};

static void __build_menu(lv_font_t *f);

static void __menu_item_tap(lv_event_t *e)
{
    const menu_item_desc_t *item = (const menu_item_desc_t *)lv_event_get_user_data(e);

    if (!item) {
        return;
    }
    if (item->scr) {
        screen_load(item->scr);
    }
}

static void __build_menu(lv_font_t *f)
{
    size_t i;

    for (i = 0; i < sizeof(s_items) / sizeof(s_items[0]); i++) {
        lv_obj_t *row;
        lv_obj_t *sym_lbl;
        lv_obj_t *lbl;

        row = lv_obj_create(ui_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_ver(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x444444), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scroll_dir(row, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, __menu_item_tap, LV_EVENT_CLICKED, (void *)&s_items[i]);

        sym_lbl = lv_label_create(row);
        lv_obj_set_style_text_font(sym_lbl, LV_FONT_DEFAULT, 0);
        lv_label_set_text(sym_lbl, s_items[i].symbol);
        lv_obj_set_style_text_color(sym_lbl, lv_color_hex(0xEEEEEE), 0);
        lv_obj_set_width(sym_lbl, 48);
        lv_obj_set_style_text_align(sym_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_right(sym_lbl, 4, 0);
        lv_obj_set_scroll_dir(sym_lbl, LV_DIR_NONE);
        lv_obj_remove_flag(sym_lbl, LV_OBJ_FLAG_CLICKABLE);

        lbl = lv_label_create(row);
        if (f) {
            lv_obj_set_style_text_font(lbl, f, 0);
        }
        lv_label_set_text(lbl, ducky_ui_str(s_items[i].text_id));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xEEEEEE), 0);
        lv_obj_set_width(lbl, lv_pct(72));
        lv_obj_set_flex_grow(lbl, 1);
        lv_obj_set_scroll_dir(lbl, LV_DIR_NONE);
        lv_obj_remove_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
    }

    ducky_scroll_arc_refresh(ui_list, 0);
}

static void screen_menu_init(void)
{
    lv_font_t *f = ai_ui_get_text_font();
    lv_obj_t * title;
    lv_obj_t * panel;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x101010), 0);
    lv_obj_remove_flag(ui_scr, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(ui_scr);
    lv_obj_set_size(panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    if (f) {
        lv_obj_set_style_text_font(panel, f, 0);
    }

    title = lv_label_create(panel);
    s_menu_title = title;
    if (f) {
        lv_obj_set_style_text_font(title, f, 0);
    }
    lv_label_set_text(title, ducky_ui_str(DUCKY_UI_STR_MENU));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    ui_list = lv_obj_create(panel);
    lv_obj_set_width(ui_list, lv_pct(100));
    lv_obj_set_flex_grow(ui_list, 1);
    lv_obj_set_style_bg_opa(ui_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_list, 0, 0);
    lv_obj_set_flex_flow(ui_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_list, LV_DIR_VER);
    ducky_scroll_arc_setup(ui_list);

    __build_menu(f);

    nav_gesture_attach(ui_scr, 0, LV_DIR_LEFT);
}

static void screen_menu_deinit(void)
{
    s_menu_title = NULL;
    ui_list = NULL;
    ui_scr = NULL;
}
