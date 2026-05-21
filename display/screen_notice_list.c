/**
 * @file screen_notice_list.c
 * @brief Recent DP101 notice bodies; open from home with downward swipe
 */
#include "screen_notice_list.h"
#include "nav_gesture.h"
#include "ui_layout.h"
#include "ai_ui_icon_font.h"
#include "ducky_notice_history.h"
#include "ui_scroll_arc.h"
#include "ducky_ui_strings.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#define DUCKY_NOTICE_LIST_ACCENT 0x00C8E8

static lv_obj_t *ui_scr;
static lv_obj_t *ui_list;

static void screen_notice_list_init(void);
static void screen_notice_list_deinit(void);

Screen_t screen_notice_list = {
    .init       = screen_notice_list_init,
    .deinit     = screen_notice_list_deinit,
    .screen_obj = &ui_scr,
    .name       = "Notices",
};

/**
 * @brief Rebuild scroll list from history
 * @return none
 */
static void __notice_fill_list(void)
{
    lv_font_t *f = ai_ui_get_text_font();
    int         n;
    int         i;

    if (!ui_list) {
        return;
    }

    n = ducky_notice_history_count();
    {
        int32_t prev_y = lv_obj_get_scroll_y(ui_list);

        lv_obj_clean(ui_list);

        if (n <= 0) {
            lv_obj_t *empty = lv_label_create(ui_list);
            if (f) {
                lv_obj_set_style_text_font(empty, f, 0);
            }
            lv_label_set_text(empty, ducky_ui_str(DUCKY_UI_STR_NOTICE_EMPTY));
            lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
            lv_obj_set_width(empty, lv_pct(92));
            ducky_scroll_arc_refresh(ui_list, prev_y);
            return;
        }

        for (i = 0; i < n; i++) {
            char        buf[DUCKY_NOTICE_HIST_TEXT_MAX];
            lv_obj_t *  row;
            lv_obj_t *  sep;
            lv_obj_t *  txt;

            if (ducky_notice_history_get(i, buf, sizeof(buf)) != OPRT_OK) {
                break;
            }

            row = lv_obj_create(ui_list);
            lv_obj_set_width(row, lv_pct(100));
            lv_obj_set_height(row, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_pad_all(row, 6, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_row(row, 4, 0);
            lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            txt = lv_label_create(row);
            if (f) {
                lv_obj_set_style_text_font(txt, f, 0);
            }
            lv_label_set_text(txt, buf);
            lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(txt, lv_color_hex(0xEEEEEE), 0);
            lv_obj_set_width(txt, lv_pct(92));
            lv_obj_set_scroll_dir(txt, LV_DIR_NONE);

            if (i + 1 < n) {
                sep = lv_obj_create(row);
                lv_obj_set_width(sep, lv_pct(92));
                lv_obj_set_height(sep, 1);
                lv_obj_set_style_bg_color(sep, lv_color_hex(0x3a3a3a), 0);
                lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(sep, 0, 0);
                lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
            }
        }

        ducky_scroll_arc_refresh(ui_list, prev_y);
    }
}

/**
 * @brief Create notice list screen
 * @return none
 */
static void screen_notice_list_init(void)
{
    lv_font_t * f = ai_ui_get_text_font();
    lv_obj_t *  panel;
    lv_obj_t *  title;
    lv_obj_t *  hint;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x1a1a1a), 0);
    lv_obj_remove_flag(ui_scr, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(ui_scr);
    lv_obj_set_size(panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 4, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    if (f) {
        lv_obj_set_style_text_font(panel, f, 0);
    }

    title = lv_label_create(panel);
    if (f) {
        lv_obj_set_style_text_font(title, f, 0);
    }
    lv_label_set_text(title, ducky_ui_str(DUCKY_UI_STR_NOTICE_LIST_TITLE));
    lv_obj_set_style_text_color(title, lv_color_hex(DUCKY_NOTICE_LIST_ACCENT), 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_remove_flag(title, LV_OBJ_FLAG_SCROLLABLE);

    ui_list = lv_obj_create(panel);
    lv_obj_set_width(ui_list, lv_pct(100));
    lv_obj_set_flex_grow(ui_list, 1);
    lv_obj_set_style_bg_opa(ui_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_list, 0, 0);
    lv_obj_set_flex_flow(ui_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_list, LV_DIR_VER);
    ducky_scroll_arc_setup(ui_list);

    hint = lv_label_create(panel);
    if (f) {
        lv_obj_set_style_text_font(hint, f, 0);
    }
    lv_label_set_text(hint, ducky_ui_str(DUCKY_UI_STR_NOTICE_HINT));
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
    lv_obj_set_width(hint, lv_pct(96));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_remove_flag(hint, LV_OBJ_FLAG_SCROLLABLE);

    __notice_fill_list();

    /* Entered from home via LV_DIR_BOTTOM: swipe up (opposite) also returns */
    nav_gesture_attach(ui_scr, 0, LV_DIR_BOTTOM);
}

/**
 * @brief Tear down
 * @return none
 */
static void screen_notice_list_deinit(void)
{
    ui_list = NULL;
    ui_scr  = NULL;
}
