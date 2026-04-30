/**
 * @file display_cron.c
 * @brief 定时任务主页 — 开机首屏
 *
 * 布局：
 *   [自定义大状态栏 75px]
 *     左侧: 大字时钟 HH:MM (2x放大)
 *     右上: WiFi图标 电池图标
 *     右下: 04/10 周四
 *   [任务列表 — 带底部分割线和复选框]
 *     [✓] PPT提交截止提醒          04/10 17:00
 *     ───────────────────────────────────────
 *     [ ] 八点看球赛提醒            04/10 20:00
 *   [页码: N-M]                (右下角)
 *
 * 已执行任务：勾选复选框 + 文字加横线（删除线）
 * RIGHT键进入菜单页面
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_cron.h"
#include "display_home.h"
#include "display_input.h"
#include "display_i18n.h"

#include "font_awesome_symbols.h"
#include "cron_service.h"
#include "tal_log.h"
#include "tal_time_service.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/***********************************************************
 ********************* 宏定义 ******************************
 ***********************************************************/
#define CRON_HEADER_HEIGHT    75      /* 自定义大状态栏高度 */
#define CRON_ITEM_HEIGHT      40      /* 每条任务的行高 */
#define CRON_ITEMS_PER_PAGE   4       /* 每页显示的任务数 */
#define CRON_REFRESH_MS       3000    /* 自动刷新检测间隔(ms) */
#define CRON_CB_SIZE          14      /* 复选框大小 */

/* 任务列表起始Y坐标（header + 1px border） */
#define CRON_LIST_TOP         (CRON_HEADER_HEIGHT + 1)
LV_FONT_DECLARE(ui_font_mont_82); /* 8.2pt 字体，约等于 16px */

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/
lv_obj_t *scr_cron = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/

/* 自定义大状态栏中的控件 */
static lv_obj_t *sg_lbl_time_big = NULL;  /* 大字时钟 HH:MM */
static lv_obj_t *sg_lbl_wifi     = NULL;  /* WiFi 图标 */
static lv_obj_t *sg_lbl_date     = NULL;  /* 日期 04/10 周四 */
static lv_obj_t *sg_lbl_battery  = NULL;  /* 电池图标 */

/* 任务列表 */
static lv_obj_t *sg_items[CRON_ITEMS_PER_PAGE] = {NULL};
static lv_obj_t *sg_lbl_page      = NULL;
static lv_obj_t *sg_lbl_no_task   = NULL;
static int32_t   sg_focus_idx   = -1;
static uint32_t  sg_cur_page    = 0;
static uint32_t  sg_total_pages = 0;
static lv_group_t *sg_group     = NULL;
static lv_timer_t *sg_refresh_timer = NULL;
static int        sg_last_job_count = -1;

/***********************************************************
 ****************** 工具函数 *******************************
 ***********************************************************/

static void __format_epoch(int64_t epoch, char *buf, size_t buf_len)
{
    if (epoch <= 0) {
        snprintf(buf, buf_len, "--/-- --:--");
        return;
    }
    POSIX_TM_S lt = {0};
    if (tal_time_get_local_time_custom((TIME_T)epoch, &lt) == OPRT_OK) {
        snprintf(buf, buf_len, "%02d/%02d %02d:%02d",
                 lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
    } else {
        snprintf(buf, buf_len, "--/-- --:--");
    }
}

/***********************************************************
 ****************** 任务项创建/更新 ************************
 ***********************************************************/

/**
 * @brief 创建一条任务行
 *
 * children:
 *   child 0: 复选框容器 (lv_obj, 14x14)
 *     child 0.0: 勾选标记 (lv_label, FONT_AWESOME_CHECK)
 *   child 1: 任务名称 (lv_label, font_puhui_14_1)
 *   child 2: 时间 (lv_label)
 *   child 3: 删除线 (lv_obj, 1px high)
 */
static lv_obj_t *__create_cron_item(lv_obj_t *parent, int y)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, DISP_HOR_RES, CRON_ITEM_HEIGHT);
    lv_obj_set_pos(row, 0, y);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_100, 0);
    lv_obj_set_style_radius(row, 0, 0);
    /* 底部分割线 — 模拟日记本横线 */
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, lv_color_make(0xC0, 0xC0, 0xC0), 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);

    /* 复选框 (child 0) */
    lv_obj_t *cb_box = lv_obj_create(row);
    lv_obj_set_size(cb_box, CRON_CB_SIZE, CRON_CB_SIZE);
    lv_obj_set_style_border_width(cb_box, 1, 0);
    lv_obj_set_style_border_color(cb_box, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_set_style_bg_color(cb_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(cb_box, LV_OPA_100, 0);
    lv_obj_set_style_radius(cb_box, 2, 0);
    lv_obj_set_style_pad_all(cb_box, 0, 0);
    lv_obj_set_align(cb_box, LV_ALIGN_LEFT_MID);
    lv_obj_remove_flag(cb_box, LV_OBJ_FLAG_SCROLLABLE);

    /* 勾选标记 (child 0.0) */
    lv_obj_t *check = lv_label_create(cb_box);
    lv_obj_set_style_text_font(check, &font_awesome_14_1, 0);
    lv_obj_set_style_text_color(check, lv_color_black(), 0);
    lv_label_set_text(check, FONT_AWESOME_CHECK);
    lv_obj_center(check);
    lv_obj_add_flag(check, LV_OBJ_FLAG_HIDDEN);

    /* 任务名称 (child 1) — font_puhui_14_1 */
    lv_obj_t *lbl_name = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_name, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);
    lv_label_set_text(lbl_name, "");
    lv_obj_set_align(lbl_name, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_name, CRON_CB_SIZE + 8, 0);
    lv_obj_set_width(lbl_name, 210);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_CLIP);

    /* 时间 (child 2) */
    lv_obj_t *lbl_time = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_time, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(lbl_time, "");
    lv_obj_set_align(lbl_time, LV_ALIGN_RIGHT_MID);

    /* 删除线 (child 3) — 覆盖在名称上，已执行时显示 */
    lv_obj_t *strike = lv_obj_create(row);
    lv_obj_set_size(strike, 1, 1);
    lv_obj_set_style_bg_color(strike, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_obj_set_style_bg_opa(strike, LV_OPA_100, 0);
    lv_obj_set_style_border_width(strike, 0, 0);
    lv_obj_set_style_radius(strike, 0, 0);
    lv_obj_set_style_pad_all(strike, 0, 0);
    lv_obj_set_align(strike, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(strike, CRON_CB_SIZE + 8, 0);
    lv_obj_remove_flag(strike, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(strike, LV_OBJ_FLAG_HIDDEN);

    return row;
}

static void __update_cron_item(lv_obj_t *row, const cron_job_t *job)
{
    if (!row || !job) return;

    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);

    /* 复选框 */
    lv_obj_t *cb_box = lv_obj_get_child(row, 0);
    lv_obj_t *check_mark = lv_obj_get_child(cb_box, 0);

    /* 任务名称 */
    lv_obj_t *lbl_name = lv_obj_get_child(row, 1);
    lv_label_set_text(lbl_name, job->name);

    /* 时间 */
    lv_obj_t *lbl_time = lv_obj_get_child(row, 2);
    char time_buf[16];
    if (job->kind == CRON_KIND_AT) {
        __format_epoch(job->at_epoch, time_buf, sizeof(time_buf));
    } else {
        __format_epoch(job->next_run, time_buf, sizeof(time_buf));
    }
    lv_label_set_text(lbl_time, time_buf);

    /* 删除线 + 复选框状态 */
    lv_obj_t *strike = lv_obj_get_child(row, 3);
    if (job->last_run > 0) {
        /* 已执行：勾选、删除线，字体颜色不变 */
        lv_obj_remove_flag(check_mark, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);
        lv_obj_set_style_text_color(lbl_time, lv_color_make(0x60, 0x60, 0x60), 0);
        lv_obj_set_size(strike, 210, 1);
        lv_obj_remove_flag(strike, LV_OBJ_FLAG_HIDDEN);
    } else {
        /* 未执行：隐藏勾选、正常颜色 */
        lv_obj_add_flag(check_mark, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);
        lv_obj_set_style_text_color(lbl_time, lv_color_make(0x60, 0x60, 0x60), 0);
        lv_obj_add_flag(strike, LV_OBJ_FLAG_HIDDEN);
    }
}

/***********************************************************
 ****************** 大状态栏刷新 ***************************
 ***********************************************************/

static void __refresh_header(void)
{
    if (!sg_lbl_time_big) return;

    POSIX_TM_S lt = {0};
    tal_time_get_local_time_custom(0, &lt);

    /* 大字时钟 */
    char clock_buf[8];
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    lv_label_set_text(sg_lbl_time_big, clock_buf);

    /* 日期 */
    int wday = (lt.tm_wday >= 0 && lt.tm_wday <= 6) ? lt.tm_wday : 0;
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%02d/%02d %s",
             lt.tm_mon + 1, lt.tm_mday, i18n_str(STR_SUN + wday));
    lv_label_set_text(sg_lbl_date, date_buf);

    /* WiFi */
    lv_label_set_text(sg_lbl_wifi, display_get_wifi_icon());

    /* 电池 */
    if (sg_lbl_battery) lv_label_set_text(sg_lbl_battery, display_get_battery_icon());
}

/***********************************************************
 ****************** 列表刷新 *******************************
 ***********************************************************/

void display_cron_refresh(void)
{
    if (!scr_cron) return;

    const cron_job_t *jobs = NULL;
    int count = 0;
    cron_list_jobs(&jobs, &count);

    /* 刷新大状态栏 */
    __refresh_header();

    /* 无任务提示 */
    if (count == 0) {
        if (sg_lbl_no_task) lv_obj_remove_flag(sg_lbl_no_task, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (sg_lbl_no_task) lv_obj_add_flag(sg_lbl_no_task, LV_OBJ_FLAG_HIDDEN);
    }

    /* 分页 */
    sg_total_pages = (count > 0) ? ((count + CRON_ITEMS_PER_PAGE - 1) / CRON_ITEMS_PER_PAGE) : 1;
    if (sg_cur_page >= sg_total_pages) sg_cur_page = sg_total_pages - 1;

    uint32_t start = sg_cur_page * CRON_ITEMS_PER_PAGE;
    int visible_count = 0;
    for (int i = 0; i < CRON_ITEMS_PER_PAGE; i++) {
        uint32_t job_idx = start + i;
        if (job_idx < (uint32_t)count) {
            __update_cron_item(sg_items[i], &jobs[job_idx]);
            visible_count++;
        } else {
            lv_obj_add_flag(sg_items[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 页码 */
    char page_buf[16];
    snprintf(page_buf, sizeof(page_buf), "%u-%u", sg_cur_page + 1, sg_total_pages);
    lv_label_set_text(sg_lbl_page, page_buf);

    if (sg_focus_idx >= visible_count) sg_focus_idx = visible_count - 1;
}

/***********************************************************
 ****************** 焦点管理 *******************************
 ***********************************************************/

static void __set_item_focus(int32_t idx, bool focused)
{
    if (idx < 0 || idx >= CRON_ITEMS_PER_PAGE || !sg_items[idx]) return;
    if (lv_obj_has_flag(sg_items[idx], LV_OBJ_FLAG_HIDDEN)) return;

    if (focused) {
        lv_obj_set_style_bg_color(sg_items[idx], lv_color_make(0xE8, 0xE8, 0xE8), 0);
    } else {
        lv_obj_set_style_bg_color(sg_items[idx], lv_color_white(), 0);
    }
}

static int __visible_count(void)
{
    int n = 0;
    for (int i = 0; i < CRON_ITEMS_PER_PAGE; i++) {
        if (sg_items[i] && !lv_obj_has_flag(sg_items[i], LV_OBJ_FLAG_HIDDEN)) n++;
    }
    return n;
}

static void __focus_move(int32_t delta)
{
    int visible = __visible_count();
    if (visible == 0) return;

    if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
    sg_focus_idx += delta;
    if (sg_focus_idx >= visible) sg_focus_idx = 0;
    if (sg_focus_idx < 0) sg_focus_idx = visible - 1;
    __set_item_focus(sg_focus_idx, true);
}

/**
 * @brief 切换当前焦点任务的完成状态
 */
static void __toggle_focused_done(void)
{
    if (sg_focus_idx < 0) return;
    int abs_idx = sg_cur_page * CRON_ITEMS_PER_PAGE + sg_focus_idx;
    cron_toggle_done(abs_idx);
    display_cron_refresh();
    __set_item_focus(sg_focus_idx, true);
}

/**
 * @brief 触摸点击任务行 — 聚焦并切换完成状态
 */
static void __item_click_cb(lv_event_t *e)
{
    int32_t idx = (int32_t)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= CRON_ITEMS_PER_PAGE) return;
    if (lv_obj_has_flag(sg_items[idx], LV_OBJ_FLAG_HIDDEN)) return;

    if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
    sg_focus_idx = idx;
    __set_item_focus(sg_focus_idx, true);
    __toggle_focused_done();
}

/***********************************************************
 ****************** 自动刷新 *******************************
 ***********************************************************/

static void __refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!scr_cron || lv_screen_active() != scr_cron) return;

    const cron_job_t *jobs = NULL;
    int count = 0;
    cron_list_jobs(&jobs, &count);

    if (count != sg_last_job_count) {
        sg_last_job_count = count;
        display_cron_refresh();
    }

    /* 刷新时钟 */
    __refresh_header();
}

/***********************************************************
 ****************** 按键事件 *******************************
 ***********************************************************/

static void __key_event_cb(lv_event_t *e)
{
    lv_key_t key = lv_event_get_key(e);
    int visible = __visible_count();

    switch (key) {
    case LV_KEY_UP:
        /* 在第一项按UP，翻到上一页最后一项 */
        if (sg_focus_idx == 0 && sg_cur_page > 0) {
            __set_item_focus(sg_focus_idx, false);
            sg_cur_page--;
            display_cron_refresh();
            int v = __visible_count();
            sg_focus_idx = v > 0 ? v - 1 : 0;
            __set_item_focus(sg_focus_idx, true);
        } else {
            __focus_move(-1);
        }
        break;
    case LV_KEY_DOWN:
        /* 在最后一项按DOWN，翻到下一页第一项 */
        if (sg_focus_idx >= 0 && sg_focus_idx >= visible - 1 &&
            sg_cur_page + 1 < sg_total_pages) {
            __set_item_focus(sg_focus_idx, false);
            sg_cur_page++;
            display_cron_refresh();
            sg_focus_idx = 0;
            __set_item_focus(sg_focus_idx, true);
        } else {
            __focus_move(1);
        }
        break;
    case LV_KEY_LEFT:
        if (sg_cur_page > 0) {
            if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
            sg_cur_page--;
            display_cron_refresh();
            sg_focus_idx = 0;
            __set_item_focus(sg_focus_idx, true);
        }
        break;
    case LV_KEY_RIGHT:
        /* 进入菜单页面 */
        display_screen_change(&scr_home, display_home_init);
        break;
    case LV_KEY_ENTER:
        /* 切换选中任务的完成状态 */
        if (sg_focus_idx < 0 && visible > 0) {
            sg_focus_idx = 0;
            __set_item_focus(sg_focus_idx, true);
        }
        __toggle_focused_done();
        break;
    default:
        break;
    }
}

static void __screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    sg_group = lv_group_create();
    lv_group_add_obj(sg_group, scr_cron);
    lv_indev_t *indev = display_input_get_keypad();
    if (indev) lv_indev_set_group(indev, sg_group);

    sg_last_job_count = -1;
    sg_cur_page = 0;
    display_cron_refresh();
    sg_focus_idx = -1;

    if (!sg_refresh_timer) {
        sg_refresh_timer = lv_timer_create(__refresh_timer_cb, CRON_REFRESH_MS, NULL);
    }
}

static void __screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCREEN_LOADED) {
        __screen_loaded_cb(e);
    } else if (code == LV_EVENT_KEY) {
        __key_event_cb(e);
    }
}

/***********************************************************
 ******************** 初始化 *******************************
 ***********************************************************/

void display_cron_init(void)
{
    if (scr_cron) return;

    /* 清理旧的刷新定时器（语言切换后可能残留） */
    if (sg_refresh_timer) {
        lv_timer_delete(sg_refresh_timer);
        sg_refresh_timer = NULL;
    }

    scr_cron = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_cron, lv_color_white(), 0);
    lv_obj_remove_flag(scr_cron, LV_OBJ_FLAG_SCROLLABLE);

    /* ===== 自定义大状态栏 (75px) ===== */
    lv_obj_t *header = lv_obj_create(scr_cron);
    lv_obj_set_size(header, DISP_HOR_RES, CRON_HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_100, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_black(), 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* 大字时钟 HH:MM（左侧居中） */
    sg_lbl_time_big = lv_label_create(header);
    lv_obj_set_style_text_font(sg_lbl_time_big, &ui_font_mont_82, 0);
    lv_obj_set_style_text_color(sg_lbl_time_big, lv_color_black(), 0);
    /* 2x 放大 */
    // lv_obj_set_style_transform_scale_x(sg_lbl_time_big, 1024, 0);
    // lv_obj_set_style_transform_scale_y(sg_lbl_time_big, 1024, 0);
    lv_label_set_text(sg_lbl_time_big, "00:00");
    lv_obj_set_align(sg_lbl_time_big, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(sg_lbl_time_big, 24, 0);

    /* WiFi 图标（右上） */
    sg_lbl_wifi = lv_label_create(header);
    lv_obj_set_style_text_font(sg_lbl_wifi, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(sg_lbl_wifi, lv_color_black(), 0);
    lv_label_set_text(sg_lbl_wifi, FONT_AWESOME_WIFI_OFF);
    lv_obj_set_align(sg_lbl_wifi, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(sg_lbl_wifi, -40, 10);

    /* 电池图标（右上，WiFi右侧） */
    sg_lbl_battery = lv_label_create(header);
    lv_obj_set_style_text_font(sg_lbl_battery, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(sg_lbl_battery, lv_color_black(), 0);
    lv_label_set_text(sg_lbl_battery, FONT_AWESOME_BATTERY_FULL);
    lv_obj_set_align(sg_lbl_battery, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(sg_lbl_battery, -8, 10);

    /* 日期: 04/10 周四（右下） */
    sg_lbl_date = lv_label_create(header);
    lv_obj_set_style_text_font(sg_lbl_date, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_date, lv_color_make(0x40, 0x40, 0x40), 0);
    lv_label_set_text(sg_lbl_date, "");
    lv_obj_set_align(sg_lbl_date, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(sg_lbl_date, -8, -10);

    /* ===== 任务列表（绝对定位） ===== */
    int y = CRON_LIST_TOP;
    for (int i = 0; i < CRON_ITEMS_PER_PAGE; i++) {
        sg_items[i] = __create_cron_item(scr_cron, y);
        lv_obj_add_event_cb(sg_items[i], __item_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        y += CRON_ITEM_HEIGHT;
    }

    /* 无任务提示 */
    sg_lbl_no_task = lv_label_create(scr_cron);
    lv_obj_set_style_text_font(sg_lbl_no_task, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_no_task, lv_color_make(0x90, 0x90, 0x90), 0);
    lv_label_set_text(sg_lbl_no_task, i18n_str(STR_NO_TASK));
    lv_obj_set_pos(sg_lbl_no_task, 16, CRON_LIST_TOP + 12);
    lv_obj_add_flag(sg_lbl_no_task, LV_OBJ_FLAG_HIDDEN);

    /* 页码标签 */
    sg_lbl_page = lv_label_create(scr_cron);
    lv_obj_set_style_text_font(sg_lbl_page, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_page, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(sg_lbl_page, "1-1");
    lv_obj_set_align(sg_lbl_page, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(sg_lbl_page, -12, -4);

    /* 注册屏幕事件 */
    lv_obj_add_event_cb(scr_cron, __screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 定时任务主页初始化完成");
}
