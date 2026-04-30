/**
 * @file display_setting.c
 * @brief 设置页面
 *
 * 布局：
 *   [状态栏: WiFi HH:MM   设置   电池]
 *   [设置列表]
 *     [light]  背光        ON/OFF
 *     [sleep]  休眠时间    5 Min
 *     [globe]  语言        中文/English
 *
 * 语言切换会销毁所有已创建页面并重建当前页面，实现全局文本切换。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_setting.h"
#include "display_status_bar.h"
#include "display_home.h"
#include "display_cron.h"
#include "display_folder.h"
#include "display_reader.h"
#include "display_notify.h"
#include "display_sleep.h"
#include "display_input.h"
#include "display_i18n.h"

#include "font_awesome_symbols.h"
#include "tal_log.h"
#include "lv_vendor.h"
#include "tuya_iot.h"

#include <string.h>
#include <stdint.h>

/***********************************************************
 ********************* 宏定义 ******************************
 ***********************************************************/
#define SETTING_ITEM_COUNT  4
#define SETTING_ITEM_HEIGHT 60

#define IDX_BACKLIGHT    0
#define IDX_SLEEP_TIME   1
#define IDX_LANGUAGE     2
#define IDX_RESET_DEVICE 3

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/
lv_obj_t *scr_setting = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/
static lv_obj_t *sg_items[SETTING_ITEM_COUNT] = {NULL};
static int32_t   sg_focus_idx = -1;
static lv_group_t *sg_group   = NULL;

/* 背光状态 */
static bool sg_backlight_on = false;

/***********************************************************
 ****************** 设置项创建 *****************************
 ***********************************************************/

static lv_obj_t *__create_setting_item(lv_obj_t *parent, const char *icon,
                                        const char *label, const char *value)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), SETTING_ITEM_HEIGHT);
    lv_obj_set_style_pad_hor(row, 12, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_100, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, lv_color_make(0xC0, 0xC0, 0xC0), 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* 图标 (child 0) */
    lv_obj_t *lbl_icon = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_icon, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_black(), 0);
    lv_label_set_text(lbl_icon, icon);
    lv_obj_set_align(lbl_icon, LV_ALIGN_LEFT_MID);

    /* 标签名 (child 1) */
    lv_obj_t *lbl_label = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_label, &font_puhui_16_2, 0);
    lv_obj_set_style_text_color(lbl_label, lv_color_black(), 0);
    lv_label_set_text(lbl_label, label);
    lv_obj_set_align(lbl_label, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_label, 30, 0);

    /* 选项值 (child 2) */
    lv_obj_t *lbl_value = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_value, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_value, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(lbl_value, value);
    lv_obj_set_align(lbl_value, LV_ALIGN_RIGHT_MID);

    return row;
}

/***********************************************************
 ****************** 选项切换逻辑 ***************************
 ***********************************************************/

static void __toggle_backlight(void)
{
    sg_backlight_on = !sg_backlight_on;
    lv_obj_t *lbl_val = lv_obj_get_child(sg_items[IDX_BACKLIGHT], 2);
    lv_label_set_text(lbl_val, sg_backlight_on ? "ON" : "OFF");
    lv_vendor_set_backlight(sg_backlight_on ? 15 : 0);
}

static void __toggle_sleep_time(void)
{
    lv_obj_t *lbl_val = lv_obj_get_child(sg_items[IDX_SLEEP_TIME], 2);
    const char *cur = lv_label_get_text(lbl_val);

    uint32_t minutes = 1;
    if (strcmp(cur, "1 Min") == 0) {
        lv_label_set_text(lbl_val, "3 Min");
        minutes = 3;
    } else if (strcmp(cur, "3 Min") == 0) {
        lv_label_set_text(lbl_val, "5 Min");
        minutes = 5;
    } else if (strcmp(cur, "5 Min") == 0) {
        lv_label_set_text(lbl_val, "15 Min");
        minutes = 15;
    } else if (strcmp(cur, "15 Min") == 0) {
        lv_label_set_text(lbl_val, "30 Min");
        minutes = 30;
    } else {
        lv_label_set_text(lbl_val, "1 Min");
        minutes = 1;
    }

    /* 同步更新深度休眠管理器的超时时间 */
    sleep_manager_set_timeout(minutes);
}

/**
 * @brief 切换语言 — 销毁所有已创建页面，重建设置页面
 */
static void __toggle_language(void)
{
    i18n_toggle_lang();

    PR_NOTICE("[setting] 语言切换为: %s", i18n_str(STR_LANG_VALUE));

    /* 重置状态栏跟踪 — 防止 status_bar_refresh 访问已释放的 LVGL 对象 */
    status_bar_reset();

    /* 销毁所有已创建的页面（除当前设置页面外） */
    if (scr_cron) { lv_obj_delete(scr_cron); scr_cron = NULL; }
    if (scr_home) { lv_obj_delete(scr_home); scr_home = NULL; }
    if (scr_folder) { lv_obj_delete(scr_folder); scr_folder = NULL; }
    if (scr_reader) { lv_obj_delete(scr_reader); scr_reader = NULL; }
    if (scr_notify_history) { lv_obj_delete(scr_notify_history); scr_notify_history = NULL; }

    /* 销毁当前设置页面并重建 */
    lv_obj_t *old = scr_setting;
    scr_setting = NULL;
    display_setting_init();
    lv_screen_load_anim(scr_setting, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, false);
    if (old) lv_obj_delete(old);
}

static void __device_reset(void)
{
    PR_NOTICE("[setting] 触发设备重置");
    tuya_iot_reset(tuya_iot_client_get());
}

static void __enter_selected(void)
{
    switch (sg_focus_idx) {
    case IDX_BACKLIGHT:    __toggle_backlight();  break;
    case IDX_SLEEP_TIME:   __toggle_sleep_time(); break;
    case IDX_LANGUAGE:     __toggle_language();   break;
    case IDX_RESET_DEVICE: __device_reset();      break;
    default: break;
    }
}

/***********************************************************
 ****************** 焦点管理 *******************************
 ***********************************************************/

static void __set_focus(int32_t idx, bool focused)
{
    if (idx < 0 || idx >= SETTING_ITEM_COUNT || !sg_items[idx]) return;

    if (focused) {
        lv_obj_set_style_border_width(sg_items[idx], 2, 0);
        lv_obj_set_style_border_color(sg_items[idx], lv_color_black(), 0);
        lv_obj_set_style_border_side(sg_items[idx], LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_radius(sg_items[idx], 4, 0);
    } else {
        lv_obj_set_style_border_width(sg_items[idx], 1, 0);
        lv_obj_set_style_border_color(sg_items[idx], lv_color_make(0xC0, 0xC0, 0xC0), 0);
        lv_obj_set_style_border_side(sg_items[idx], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_radius(sg_items[idx], 0, 0);
    }
}

static void __focus_move(int32_t delta)
{
    if (sg_focus_idx >= 0) __set_focus(sg_focus_idx, false);
    sg_focus_idx += delta;
    if (sg_focus_idx >= SETTING_ITEM_COUNT) sg_focus_idx = 0;
    if (sg_focus_idx < 0) sg_focus_idx = SETTING_ITEM_COUNT - 1;
    __set_focus(sg_focus_idx, true);
}

static void __setting_item_click_cb(lv_event_t *e)
{
    int32_t idx = (int32_t)(intptr_t)lv_event_get_user_data(e);
    if (sg_focus_idx >= 0) __set_focus(sg_focus_idx, false);
    sg_focus_idx = idx;
    __set_focus(sg_focus_idx, true);
    __enter_selected();
}

/***********************************************************
 ****************** 按键事件 *******************************
 ***********************************************************/

static void __screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED) {
        sg_group = lv_group_create();
        lv_group_add_obj(sg_group, scr_setting);
        lv_indev_t *indev = display_input_get_keypad();
        if (indev) lv_indev_set_group(indev, sg_group);

        sg_focus_idx = -1;
        __focus_move(1);
    } else if (code == LV_EVENT_KEY) {
        lv_key_t key = lv_event_get_key(e);
        switch (key) {
        case LV_KEY_UP:   __focus_move(-1); break;
        case LV_KEY_DOWN: __focus_move(1);  break;
        case LV_KEY_ENTER: __enter_selected(); break;
        case LV_KEY_ESC:
            /* 返回菜单页面 */
            display_screen_change(&scr_home, display_home_init);
            break;
        default: break;
        }
    }
}

static void __status_bar_click_cb(lv_event_t *e)
{
    (void)e;
    display_screen_change(&scr_home, display_home_init);
}

/***********************************************************
 ******************** 初始化 *******************************
 ***********************************************************/

void display_setting_init(void)
{
    if (scr_setting) return;

    scr_setting = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_setting, lv_color_white(), 0);
    lv_obj_remove_flag(scr_setting, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏 */
    lv_obj_t *bar = status_bar_create(scr_setting, i18n_str(STR_SETTING));
    lv_obj_add_event_cb(bar, __status_bar_click_cb, LV_EVENT_CLICKED, NULL);

    /* 设置列表容器 */
    lv_obj_t *list = lv_obj_create(scr_setting);
    lv_obj_set_size(list, DISP_HOR_RES, CONTENT_HEIGHT);
    lv_obj_set_pos(list, 0, STATUS_BAR_HEIGHT);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    /* 设置项 */
    sg_items[IDX_BACKLIGHT]    = __create_setting_item(list, FONT_AWESOME_IMAGE,
                                   i18n_str(STR_BACKLIGHT), sg_backlight_on ? "ON" : "OFF");
    sg_items[IDX_SLEEP_TIME]   = __create_setting_item(list, FONT_AWESOME_BELL,
                                   i18n_str(STR_SLEEP_TIME), "5 Min");
    sg_items[IDX_LANGUAGE]     = __create_setting_item(list, FONT_AWESOME_GLOBE,
                                   i18n_str(STR_LANGUAGE), i18n_str(STR_LANG_VALUE));
    sg_items[IDX_RESET_DEVICE] = __create_setting_item(list, FONT_AWESOME_POWER,
                                   i18n_str(STR_RESET_DEVICE), "");

    /* 触摸点击事件 */
    for (int i = 0; i < SETTING_ITEM_COUNT; i++) {
        lv_obj_add_event_cb(sg_items[i], __setting_item_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    lv_obj_add_event_cb(scr_setting, __screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 设置页面初始化完成");
}
