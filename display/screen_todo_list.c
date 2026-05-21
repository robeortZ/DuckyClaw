/**
 * @file screen_todo_list.c
 * @brief Todo / reminders list (round display safe area)
 */
#include "screen_todo_list.h"
#include "nav_gesture.h"
#include "ui_layout.h"
#include "ai_ui_icon_font.h"
#include "todo_service.h"
#include "ui_scroll_arc.h"
#include "ducky_ui_strings.h"

#include "lvgl.h"
#include "tal_api.h"

#include <stdio.h>
#include <string.h>

#define DUCKY_TODO_ORANGE   0xFF6B00
#define DUCKY_TODO_SYNC_MS  1500

static lv_obj_t *  ui_scr;
static lv_obj_t *  ui_panel;
static lv_obj_t *  ui_list;
static lv_timer_t *sg_sync_timer;

static void screen_todo_init(void);
static void screen_todo_deinit(void);
static void __todo_fill_list(void);
static void __todo_sync_cb(lv_timer_t *t);
static void __async_rebuild(void *ud);
static void __on_todo_cb(lv_event_t *e);
static void __cb_ud_free(lv_event_t *e);

Screen_t screen_todo = {
    .init = screen_todo_init,
    .deinit = screen_todo_deinit,
    .screen_obj = &ui_scr,
    .name = "Todo",
};

static void __cb_ud_free(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    {
        void *p = lv_obj_get_user_data(lv_event_get_target(e));
        if (p) {
            tal_free(p);
        }
    }
}

static void __async_rebuild(void *ud)
{
    (void)ud;
    __todo_fill_list();
}

static void __on_todo_cb(lv_event_t *e)
{
    lv_obj_t *cb = lv_event_get_target(e);
    char *    id = (char *)lv_obj_get_user_data(cb);

    if (!id) {
        return;
    }
    {
        bool done = lv_obj_has_state(cb, LV_STATE_CHECKED);
        (void)todo_set_completed_by_id(id, done);
    }
    if (lv_async_call(__async_rebuild, NULL) != LV_RESULT_OK) {
        __todo_fill_list();
    }
}

static void __todo_sync_cb(lv_timer_t *t)
{
    (void)t;
    __todo_fill_list();
}

static void __todo_fill_list(void)
{
    lv_font_t *   f = ai_ui_get_text_font();
    const todo_task_t *tasks = NULL;
    int            n = 0;
    int            pending = 0;

    if (!ui_list) {
        return;
    }

    todo_list_tasks(&tasks, &n);
    for (int i = 0; i < n; i++) {
        if (!tasks[i].completed) {
            pending++;
        }
    }

    {
        int32_t prev_scroll_y = lv_obj_get_scroll_y(ui_list);

        lv_obj_clean(ui_list);

        if (pending <= 0) {
            lv_obj_t *empty = lv_label_create(ui_list);
            if (f) {
                lv_obj_set_style_text_font(empty, f, 0);
            }
            lv_label_set_text(empty, ducky_ui_str(DUCKY_UI_STR_TODO_EMPTY));
            lv_obj_set_style_text_color(empty, lv_color_hex(0x666666), 0);
            ducky_scroll_arc_refresh(ui_list, prev_scroll_y);
            return;
        }

        for (int i = 0; i < n; i++) {
        const todo_task_t *t = &tasks[i];
        lv_obj_t *         row;
        lv_obj_t *         cb;
        lv_obj_t *         txt;
        char *             idcpy;
        size_t             idl;

        if (t->completed) {
            continue;
        }

        row = lv_obj_create(ui_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_ver(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 2, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scroll_dir(row, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        cb = lv_checkbox_create(row);
        lv_checkbox_set_text(cb, "");
        if (f) {
            lv_obj_set_style_text_font(cb, f, 0);
        }
        lv_obj_set_style_pad_all(cb, 0, 0);
        lv_obj_set_style_radius(cb, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_radius(cb, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(cb, 2, LV_PART_INDICATOR);
        lv_obj_set_style_border_color(cb, lv_color_hex(0xBBBBBB), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(cb, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, lv_color_hex(DUCKY_TODO_ORANGE), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, __cb_ud_free, LV_EVENT_DELETE, NULL);

        idl = strlen(t->id) + 1;
        idcpy = (char *)tal_malloc(idl);
        if (idcpy) {
            memcpy(idcpy, t->id, idl);
            lv_obj_set_user_data(cb, idcpy);
        }
        lv_obj_add_event_cb(cb, __on_todo_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_set_scroll_dir(cb, LV_DIR_NONE);

        txt = lv_label_create(row);
        if (f) {
            lv_obj_set_style_text_font(txt, f, 0);
        }
        lv_label_set_text(txt, t->content);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(txt, lv_color_hex(0x111111), 0);
        lv_obj_set_width(txt, lv_pct(78));
        lv_obj_set_flex_grow(txt, 1);
        lv_obj_set_scroll_dir(txt, LV_DIR_NONE);
        }

        ducky_scroll_arc_refresh(ui_list, prev_scroll_y);
    }
}

static void screen_todo_init(void)
{
    lv_font_t *f = ai_ui_get_text_font();
    lv_obj_t * title;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0xFFFFFF), 0);

    ui_panel = lv_obj_create(ui_scr);
    lv_obj_set_size(ui_panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(ui_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_panel, 0, 0);
    lv_obj_set_flex_flow(ui_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui_panel, 8, 0);
    lv_obj_set_style_pad_all(ui_panel, 4, 0);
    if (f) {
        lv_obj_set_style_text_font(ui_panel, f, 0);
    }

    title = lv_label_create(ui_panel);
    if (f) {
        lv_obj_set_style_text_font(title, f, 0);
    }
    lv_label_set_text(title, ducky_ui_str(DUCKY_UI_STR_TODO_TITLE));
    lv_obj_set_style_text_color(title, lv_color_hex(DUCKY_TODO_ORANGE), 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    ui_list = lv_obj_create(ui_panel);
    lv_obj_set_width(ui_list, lv_pct(100));
    lv_obj_set_flex_grow(ui_list, 1);
    lv_obj_set_style_bg_opa(ui_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_list, 0, 0);
    lv_obj_set_flex_flow(ui_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_list, LV_DIR_VER);
    ducky_scroll_arc_setup(ui_list);

    __todo_fill_list();
    sg_sync_timer = lv_timer_create(__todo_sync_cb, DUCKY_TODO_SYNC_MS, NULL);

    nav_gesture_attach(ui_scr, 0, LV_DIR_NONE);
}

static void screen_todo_deinit(void)
{
    if (sg_sync_timer) {
        lv_timer_del(sg_sync_timer);
        sg_sync_timer = NULL;
    }
    ui_list = NULL;
    ui_panel = NULL;
    ui_scr = NULL;
}
