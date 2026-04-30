/**
 * @file display_notify.c
 * @brief 通知弹窗 + 历史通知浏览
 *
 * 弹窗布局（全屏 400x300）：
 *   [标题: 通知]
 *   [内容 — 居中]
 *   [时间 — 右下角]
 *
 * 历史通知页面：一条通知占一个页面，左/右翻页
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_notify.h"
#include "display_status_bar.h"
#include "display_home.h"
#include "display_input.h"
#include "display_i18n.h"

#include "font_awesome_symbols.h"
#include "tal_log.h"
#include "tal_time_service.h"
#include "lv_vendor.h"
#include "ai_audio_player.h"

#include <stdio.h>
#include <string.h>

/***********************************************************
 ********************* 历史通知存储 ************************
 ***********************************************************/

typedef struct {
    char title[NOTIFY_TITLE_MAX];
    char content[NOTIFY_CONTENT_MAX];
    char time_str[20]; /* MM/DD HH:MM */
} notify_record_t;

static notify_record_t sg_history[NOTIFY_MAX_HISTORY];
static int sg_history_count = 0;
static int sg_history_write = 0; /* 环形写入位置 */

static void __save_to_history(const char *title, const char *content, const char *time_str)
{
    notify_record_t *rec = &sg_history[sg_history_write];
    strncpy(rec->title, title, NOTIFY_TITLE_MAX - 1);
    rec->title[NOTIFY_TITLE_MAX - 1] = '\0';
    strncpy(rec->content, content, NOTIFY_CONTENT_MAX - 1);
    rec->content[NOTIFY_CONTENT_MAX - 1] = '\0';
    strncpy(rec->time_str, time_str, sizeof(rec->time_str) - 1);
    rec->time_str[sizeof(rec->time_str) - 1] = '\0';

    sg_history_write = (sg_history_write + 1) % NOTIFY_MAX_HISTORY;
    if (sg_history_count < NOTIFY_MAX_HISTORY) {
        sg_history_count++;
    }
}

/**
 * @brief 获取第 idx 条历史通知（0=最新）
 */
static const notify_record_t *__get_history(int idx)
{
    if (idx < 0 || idx >= sg_history_count) return NULL;
    int pos = (sg_history_write - 1 - idx + NOTIFY_MAX_HISTORY) % NOTIFY_MAX_HISTORY;
    return &sg_history[pos];
}

/***********************************************************
 ********************* 弹窗 *******************************
 ***********************************************************/

static lv_obj_t *sg_popup = NULL;
static lv_group_t *sg_popup_group = NULL;

/* 弹窗上一个屏幕（关闭后恢复） */
static lv_obj_t *sg_prev_screen = NULL;

static void __close_popup(void)
{
    if (!sg_popup) return;

    if (sg_prev_screen) {
        lv_screen_load_anim(sg_prev_screen, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, false);
    }
    sg_popup = NULL;
    sg_prev_screen = NULL;
}

static void __popup_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_KEY || code == LV_EVENT_CLICKED) {
        __close_popup();
    } else if (code == LV_EVENT_SCREEN_LOADED) {
        sg_popup_group = lv_group_create();
        lv_group_add_obj(sg_popup_group, sg_popup);
        lv_indev_t *indev = display_input_get_keypad();
        if (indev) lv_indev_set_group(indev, sg_popup_group);
    }
}

void display_notify_show(const char *title, const char *content)
{
    if (!content || content[0] == '\0') return;
    if (!title || title[0] == '\0') title = i18n_str(STR_NOTIFICATION);

    /* 获取当前时间字符串 */
    char time_str[20] = {0};
    POSIX_TM_S lt = {0};
    tal_time_get_local_time_custom(0, &lt);
    snprintf(time_str, sizeof(time_str), "%02d/%02d %02d:%02d",
             lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);

    /* 保存到历史 */
    __save_to_history(title, content, time_str);

    PR_DEBUG("[notify] 收到通知: [%s] %s", title, content);

    /* 播放提示音 */
    ai_audio_player_alert(AI_AUDIO_ALERT_CLOCK);

    /* 创建全屏弹窗 */
    lv_vendor_disp_lock();

    if (sg_popup) {
        /* 已有弹窗 — 删除旧弹窗，保留原始 sg_prev_screen */
        lv_obj_delete(sg_popup);
        sg_popup = NULL;
    } else {
        sg_prev_screen = lv_screen_active();
    }

    sg_popup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(sg_popup, lv_color_white(), 0);
    lv_obj_remove_flag(sg_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *lbl_title = lv_label_create(sg_popup);
    lv_obj_set_style_text_font(lbl_title, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_black(), 0);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_width(lbl_title, DISP_HOR_RES - 40);
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_align(lbl_title, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(lbl_title, 0, 16);

    /* 分割线 */
    lv_obj_t *line = lv_obj_create(sg_popup);
    lv_obj_set_size(line, DISP_HOR_RES - 40, 1);
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_100, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_align(line, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(line, 0, 42);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    /* 内容 — 居中 */
    lv_obj_t *lbl_content = lv_label_create(sg_popup);
    lv_obj_set_style_text_font(lbl_content, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_content, lv_color_black(), 0);
    lv_obj_set_width(lbl_content, DISP_HOR_RES - 40);
    lv_label_set_long_mode(lbl_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lbl_content, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_content, content);
    lv_obj_set_align(lbl_content, LV_ALIGN_CENTER);
    lv_obj_set_pos(lbl_content, 0, 10);

    /* 时间 — 右下角 */
    lv_obj_t *lbl_time = lv_label_create(sg_popup);
    lv_obj_set_style_text_font(lbl_time, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(lbl_time, time_str);
    lv_obj_set_align(lbl_time, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl_time, -16, -12);

    /* 注册事件：任意键或触摸关闭 */
    lv_obj_add_event_cb(sg_popup, __popup_event_cb, LV_EVENT_ALL, NULL);

    /* 加载弹窗屏幕 */
    lv_screen_load_anim(sg_popup, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, false);

    lv_vendor_disp_unlock();
    tal_system_sleep(3000); /* 弹窗至少显示 3 秒 */
    /* 播放提示音 */
    ai_audio_player_alert(AI_AUDIO_ALERT_CLOCK);
}

/***********************************************************
 ********************* 历史通知页面 ************************
 ***********************************************************/

lv_obj_t *scr_notify_history = NULL;

static lv_obj_t *sg_hist_lbl_title   = NULL;
static lv_obj_t *sg_hist_lbl_content = NULL;
static lv_obj_t *sg_hist_lbl_time    = NULL;
static lv_obj_t *sg_hist_lbl_page    = NULL;
static int sg_hist_cur_page = 0;
static lv_group_t *sg_hist_group = NULL;

static void __hist_show_page(void)
{
    if (sg_history_count == 0) {
        if (sg_hist_lbl_title) lv_label_set_text(sg_hist_lbl_title, i18n_str(STR_NOTIFICATION));
        lv_label_set_text(sg_hist_lbl_content, i18n_str(STR_NO_NOTIF));
        lv_label_set_text(sg_hist_lbl_time, "");
        lv_label_set_text(sg_hist_lbl_page, "0/0");
        return;
    }

    if (sg_hist_cur_page >= sg_history_count) {
        sg_hist_cur_page = sg_history_count - 1;
    }
    if (sg_hist_cur_page < 0) sg_hist_cur_page = 0;

    const notify_record_t *rec = __get_history(sg_hist_cur_page);
    if (rec) {
        if (sg_hist_lbl_title) lv_label_set_text(sg_hist_lbl_title, rec->title);
        lv_label_set_text(sg_hist_lbl_content, rec->content);
        lv_label_set_text(sg_hist_lbl_time, rec->time_str);
    }

    char page_buf[16];
    snprintf(page_buf, sizeof(page_buf), "%d/%d", sg_hist_cur_page + 1, sg_history_count);
    lv_label_set_text(sg_hist_lbl_page, page_buf);
}

static void __hist_status_bar_click_cb(lv_event_t *e)
{
    (void)e;
    display_screen_change(&scr_home, display_home_init);
}

static void __hist_screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED) {
        sg_hist_group = lv_group_create();
        lv_group_add_obj(sg_hist_group, scr_notify_history);
        lv_indev_t *indev = display_input_get_keypad();
        if (indev) lv_indev_set_group(indev, sg_hist_group);

        sg_hist_cur_page = 0;
        __hist_show_page();
    } else if (code == LV_EVENT_KEY) {
        lv_key_t key = lv_event_get_key(e);
        switch (key) {
        case LV_KEY_LEFT:
        case LV_KEY_UP:
            if (sg_hist_cur_page > 0) {
                sg_hist_cur_page--;
                __hist_show_page();
            }
            break;
        case LV_KEY_RIGHT:
        case LV_KEY_DOWN:
            if (sg_hist_cur_page + 1 < sg_history_count) {
                sg_hist_cur_page++;
                __hist_show_page();
            }
            break;
        case LV_KEY_ESC:
            display_screen_change(&scr_home, display_home_init);
            break;
        default:
            break;
        }
    }
}

void display_notify_history_init(void)
{
    if (scr_notify_history) return;

    scr_notify_history = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_notify_history, lv_color_white(), 0);
    lv_obj_remove_flag(scr_notify_history, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏 */
    lv_obj_t *bar = status_bar_create(scr_notify_history, i18n_str(STR_HISTORY));
    lv_obj_add_event_cb(bar, __hist_status_bar_click_cb, LV_EVENT_CLICKED, NULL);

    /* 标题（每条通知自己的标题） */
    sg_hist_lbl_title = lv_label_create(scr_notify_history);
    lv_obj_set_style_text_font(sg_hist_lbl_title, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_hist_lbl_title, lv_color_black(), 0);
    lv_obj_set_width(sg_hist_lbl_title, DISP_HOR_RES - 40);
    lv_label_set_long_mode(sg_hist_lbl_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(sg_hist_lbl_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_hist_lbl_title, i18n_str(STR_NOTIFICATION));
    lv_obj_set_align(sg_hist_lbl_title, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(sg_hist_lbl_title, 0, STATUS_BAR_HEIGHT + 8);

    /* 分割线 */
    lv_obj_t *line = lv_obj_create(scr_notify_history);
    lv_obj_set_size(line, DISP_HOR_RES - 40, 1);
    lv_obj_set_style_bg_color(line, lv_color_make(0xC0, 0xC0, 0xC0), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_100, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_align(line, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(line, 0, STATUS_BAR_HEIGHT + 34);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    /* 内容 — 居中显示 */
    sg_hist_lbl_content = lv_label_create(scr_notify_history);
    lv_obj_set_style_text_font(sg_hist_lbl_content, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_hist_lbl_content, lv_color_black(), 0);
    lv_obj_set_width(sg_hist_lbl_content, DISP_HOR_RES - 40);
    lv_label_set_long_mode(sg_hist_lbl_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sg_hist_lbl_content, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(sg_hist_lbl_content, "");
    lv_obj_set_align(sg_hist_lbl_content, LV_ALIGN_CENTER);
    lv_obj_set_pos(sg_hist_lbl_content, 0, 10);

    /* 时间 — 右下角 */
    sg_hist_lbl_time = lv_label_create(scr_notify_history);
    lv_obj_set_style_text_font(sg_hist_lbl_time, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_hist_lbl_time, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(sg_hist_lbl_time, "");
    lv_obj_set_align(sg_hist_lbl_time, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(sg_hist_lbl_time, -16, -24);

    /* 页码 */
    sg_hist_lbl_page = lv_label_create(scr_notify_history);
    lv_obj_set_style_text_font(sg_hist_lbl_page, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_hist_lbl_page, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(sg_hist_lbl_page, "0/0");
    lv_obj_set_align(sg_hist_lbl_page, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_pos(sg_hist_lbl_page, 16, -24);

    /* 注册屏幕事件 */
    lv_obj_add_event_cb(scr_notify_history, __hist_screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 历史通知页面初始化完成");
}
