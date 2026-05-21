/**
 * @file screen_cron_list.c
 * @brief Cron / timer list UI (rows, end time, periodic sync)
 */
#include "screen_cron_list.h"
#include "nav_gesture.h"
#include "ui_layout.h"
#include "cron_service.h"
#include "ai_ui_icon_font.h"
#include "ui_scroll_arc.h"
#include "ducky_ui_strings.h"

#include "lvgl.h"
#include "tal_api.h"
#include "tal_time_service.h"

#include <stdio.h>
#include <string.h>

#define CRON_SYNC_MS 1800

static lv_obj_t *  ui_scr;
static lv_obj_t *  ui_panel;
static lv_obj_t *  ui_list;
static lv_timer_t *sg_cron_timer;

static void screen_cron_init(void);
static void screen_cron_deinit(void);
static void __cron_refresh_list(void);
static void __cron_timer_cb(lv_timer_t *t);
static void __cron_fmt_time(const cron_job_t *j, char *buf, size_t len);

Screen_t screen_cron = {
    .init = screen_cron_init,
    .deinit = screen_cron_deinit,
    .screen_obj = &ui_scr,
    .name = "Cron",
};

static void __cron_fmt_time(const cron_job_t *j, char *buf, size_t len)
{
    int64_t ts;

    if (!buf || len < 6) {
        return;
    }
    if (j->kind == CRON_KIND_AT) {
        ts = j->at_epoch;
    } else {
        ts = j->next_run;
    }
    if (ts <= 0) {
        snprintf(buf, len, "--:--");
        return;
    }
    {
        TIME_T       tt = (TIME_T)ts;
        POSIX_TM_S   tm;

        memset(&tm, 0, sizeof(tm));
        tal_time_get_local_time_custom(tt, &tm);
        snprintf(buf, len, "%02d:%02d", tm.tm_hour, tm.tm_min);
    }
}

static void __cron_refresh_list(void)
{
    lv_font_t *        f = ai_ui_get_text_font();
    const cron_job_t * jobs = NULL;
    int                cnt = 0;

    if (!ui_list) {
        return;
    }

    {
        int32_t prev_scroll_y = lv_obj_get_scroll_y(ui_list);

        lv_obj_clean(ui_list);

        cron_list_jobs(&jobs, &cnt);

        if (!jobs || cnt <= 0) {
            lv_obj_t *lbl = lv_label_create(ui_list);
            if (f) {
                lv_obj_set_style_text_font(lbl, f, 0);
            }
            lv_label_set_text(lbl, ducky_ui_str(DUCKY_UI_STR_CRON_EMPTY));
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
            ducky_scroll_arc_refresh(ui_list, prev_scroll_y);
            return;
        }

        for (int i = 0; i < cnt; i++) {
        const cron_job_t *j = &jobs[i];
        lv_obj_t *        row;
        lv_obj_t *        left;
        lv_obj_t *        ln;
        lv_obj_t *        lm;
        lv_obj_t *        tr;
        char              tbuf[12];

        row = lv_obj_create(ui_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_ver(row, 6, 0);
        lv_obj_set_style_pad_hor(row, 2, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x444444), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scroll_dir(row, LV_DIR_NONE);

        left = lv_obj_create(row);
        lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(left, 0, 0);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_style_pad_row(left, 2, 0);
        lv_obj_set_scroll_dir(left, LV_DIR_NONE);

        ln = lv_label_create(left);
        if (f) {
            lv_obj_set_style_text_font(ln, f, 0);
        }
        lv_label_set_text(ln, j->name);
        lv_label_set_long_mode(ln, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(ln, lv_color_hex(0xEEEEEE), 0);
        lv_obj_set_width(ln, lv_pct(100));
        lv_obj_set_scroll_dir(ln, LV_DIR_NONE);

        lm = lv_label_create(left);
        if (f) {
            lv_obj_set_style_text_font(lm, f, 0);
        }
        lv_label_set_text(lm, j->message);
        lv_label_set_long_mode(lm, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(lm, lv_color_hex(0x999999), 0);
        lv_obj_set_width(lm, lv_pct(100));
        lv_obj_set_scroll_dir(lm, LV_DIR_NONE);

        tr = lv_label_create(row);
        if (f) {
            lv_obj_set_style_text_font(tr, f, 0);
        }
        lv_obj_set_scroll_dir(tr, LV_DIR_NONE);
        __cron_fmt_time(j, tbuf, sizeof(tbuf));
        lv_label_set_text(tr, tbuf);
        lv_obj_set_style_text_color(tr, lv_color_hex(0xFF9933), 0);

        if (!j->enabled) {
            lv_obj_set_style_text_opa(ln, LV_OPA_50, 0);
            lv_obj_set_style_text_opa(lm, LV_OPA_50, 0);
            lv_obj_set_style_text_opa(tr, LV_OPA_50, 0);
        }
        }

        ducky_scroll_arc_refresh(ui_list, prev_scroll_y);
    }
}

static void __cron_timer_cb(lv_timer_t *t)
{
    (void)t;
    __cron_refresh_list();
}

static void screen_cron_init(void)
{
    lv_font_t *f = ai_ui_get_text_font();
    lv_obj_t * title;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x101010), 0);

    ui_panel = lv_obj_create(ui_scr);
    lv_obj_set_size(ui_panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(ui_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(ui_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_panel, 0, 0);
    lv_obj_set_flex_flow(ui_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui_panel, 8, 0);
    if (f) {
        lv_obj_set_style_text_font(ui_panel, f, 0);
    }

    title = lv_label_create(ui_panel);
    if (f) {
        lv_obj_set_style_text_font(title, f, 0);
    }
    lv_label_set_text(title, ducky_ui_str(DUCKY_UI_STR_CRON_TITLE));
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
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

    __cron_refresh_list();
    sg_cron_timer = lv_timer_create(__cron_timer_cb, CRON_SYNC_MS, NULL);

    nav_gesture_attach(ui_scr, 0, LV_DIR_NONE);
}

static void screen_cron_deinit(void)
{
    if (sg_cron_timer) {
        lv_timer_del(sg_cron_timer);
        sg_cron_timer = NULL;
    }
    ui_list = NULL;
    ui_panel = NULL;
    ui_scr = NULL;
}
