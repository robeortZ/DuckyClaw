/**
 * @file display_status_bar.c
 * @brief 状态栏组件 — 时间、页面标题、WiFi、电池
 *
 * 布局（400x30）：
 * [WiFi图标] HH:MM        页面标题  [电池图标]
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_status_bar.h"
#include "font_awesome_symbols.h"
#include "tal_log.h"
#include "tal_time_service.h"
#include "lv_vendor.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/

/* 最多支持的页面数（主页 + 子页面） */
#define STATUS_BAR_MAX 5

/* 每个状态栏实例的子控件引用 */
typedef struct {
    lv_obj_t *bar;          /* 状态栏容器 */
    lv_obj_t *lbl_wifi;     /* WiFi 图标 */
    lv_obj_t *lbl_time;     /* 时间 HH:MM */
    lv_obj_t *lbl_title;    /* 页面标题 */
    lv_obj_t *lbl_battery;  /* 电池图标 */
} status_bar_inst_t;

static status_bar_inst_t sg_bars[STATUS_BAR_MAX];
static int sg_bar_count = 0;
static lv_timer_t *sg_timer = NULL;  /* 刷新定时器 */

/***********************************************************
 ******************* 内部函数 ******************************
 ***********************************************************/

static void __status_bar_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    status_bar_refresh();
}

/***********************************************************
 ********************* 公共函数 ****************************
 ***********************************************************/

lv_obj_t *status_bar_create(lv_obj_t *parent, const char *title)
{
    if (sg_bar_count >= STATUS_BAR_MAX) {
        PR_WARN("[status_bar] 实例数已满 (%d)", STATUS_BAR_MAX);
        return NULL;
    }

    status_bar_inst_t *inst = &sg_bars[sg_bar_count];

    /* 状态栏容器 */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, DISP_HOR_RES, STATUS_BAR_HEIGHT);
    lv_obj_set_align(bar, LV_ALIGN_TOP_MID);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_100, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, lv_color_black(), 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    inst->bar = bar;

    /* WiFi 图标（左侧） */
    inst->lbl_wifi = lv_label_create(bar);
    lv_obj_set_style_text_font(inst->lbl_wifi, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(inst->lbl_wifi, lv_color_black(), 0);
    lv_label_set_text(inst->lbl_wifi, FONT_AWESOME_WIFI_OFF);
    lv_obj_set_align(inst->lbl_wifi, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(inst->lbl_wifi, 2, 0);

    /* 时间 HH:MM（WiFi 图标右侧） */
    inst->lbl_time = lv_label_create(bar);
    lv_obj_set_style_text_font(inst->lbl_time, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(inst->lbl_time, lv_color_black(), 0);
    lv_label_set_text(inst->lbl_time, "00:00");
    lv_obj_set_align(inst->lbl_time, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(inst->lbl_time, 26, 0);

    /* 页面标题（居中） */
    inst->lbl_title = lv_label_create(bar);
    lv_obj_set_style_text_font(inst->lbl_title, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(inst->lbl_title, lv_color_black(), 0);
    lv_label_set_text(inst->lbl_title, title ? title : "");
    lv_obj_set_align(inst->lbl_title, LV_ALIGN_CENTER);
    lv_obj_set_pos(inst->lbl_title, 0, 0);

    /* 电池图标（右侧） */
    inst->lbl_battery = lv_label_create(bar);
    lv_obj_set_style_text_font(inst->lbl_battery, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(inst->lbl_battery, lv_color_black(), 0);
    lv_label_set_text(inst->lbl_battery, FONT_AWESOME_BATTERY_FULL);
    lv_obj_set_align(inst->lbl_battery, LV_ALIGN_RIGHT_MID);
    lv_obj_set_pos(inst->lbl_battery, -2, 0);

    sg_bar_count++;

    /* 立即刷新一次 */
    status_bar_refresh();

    return bar;
}

void status_bar_refresh(void)
{
    if (sg_bar_count <= 0) {
        return;
    }

    lv_vendor_disp_lock();

    /* 获取当前本地时间 */
    POSIX_TM_S local_time = {0};
    tal_time_get_local_time_custom(0, &local_time);

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", local_time.tm_hour, local_time.tm_min);

    const char *wifi_icon = display_get_wifi_icon();
    const char *battery_icon = display_get_battery_icon();

    /* 刷新所有状态栏实例 */
    for (int i = 0; i < sg_bar_count; i++) {
        status_bar_inst_t *inst = &sg_bars[i];
        if (inst->lbl_time) lv_label_set_text(inst->lbl_time, time_buf);
        if (inst->lbl_wifi) lv_label_set_text(inst->lbl_wifi, wifi_icon);
        if (inst->lbl_battery) lv_label_set_text(inst->lbl_battery, battery_icon);
    }

    lv_vendor_disp_unlock();
}

void status_bar_set_title(lv_obj_t *bar, const char *title)
{
    for (int i = 0; i < sg_bar_count; i++) {
        if (sg_bars[i].bar == bar && sg_bars[i].lbl_title) {
            lv_label_set_text(sg_bars[i].lbl_title, title ? title : "");
            return;
        }
    }
}

void status_bar_reset(void)
{
    sg_bar_count = 0;
    memset(sg_bars, 0, sizeof(sg_bars));
}

void status_bar_timer_start(void)
{
    if (!sg_timer) {
        sg_timer = lv_timer_create(__status_bar_timer_cb, 30000, NULL);
    }
}
