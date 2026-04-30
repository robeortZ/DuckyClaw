/**
 * @file display_home.c
 * @brief 菜单页面（原主页，现为二级菜单）
 *
 * 布局：
 *   [状态栏: WiFi HH:MM   菜单   电池]
 *   [菜单列表 - 纵向排列]
 *     [bell]   定时任务    >
 *     [folder] 文件夹      >
 *     [bell]   历史通知    >
 *     [gear]   设置        >
 *
 * 按键：上/下切换焦点，回车进入子页面，ESC返回主页(定时任务)
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_home.h"
#include "display_status_bar.h"
#include "display_input.h"
#include "display_cron.h"
#include "display_folder.h"
#include "display_setting.h"
#include "display_notify.h"
#include "display_i18n.h"

#include "font_awesome_symbols.h"
#include "tal_log.h"

#include <stdint.h>

/***********************************************************
 ********************* 宏定义 ******************************
 ***********************************************************/
#define HOME_MENU_ITEM_COUNT 4
#define HOME_MENU_ITEM_HEIGHT 55

/* 菜单项索引 */
#define IDX_CRON    0
#define IDX_FOLDER  1
#define IDX_NOTIFY  2
#define IDX_SETTING 3

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/
lv_obj_t *scr_home = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/
static lv_obj_t *sg_menu_items[HOME_MENU_ITEM_COUNT] = {NULL};
static int32_t   sg_focus_idx = -1;
static lv_group_t *sg_group = NULL;

/***********************************************************
 ****************** 菜单项创建 *****************************
 ***********************************************************/

static lv_obj_t *__create_menu_item(lv_obj_t *parent, const char *icon,
                                    const char *text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), HOME_MENU_ITEM_HEIGHT);
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

    /* 文字标签 (child 1) */
    lv_obj_t *lbl_text = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_text, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_text, lv_color_black(), 0);
    lv_label_set_text(lbl_text, text);
    lv_obj_set_align(lbl_text, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_text, 30, 0);

    /* 右箭头 (child 2) */
    lv_obj_t *lbl_arrow = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_arrow, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(lbl_arrow, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_label_set_text(lbl_arrow, FONT_AWESOME_ARROW_RIGHT);
    lv_obj_set_align(lbl_arrow, LV_ALIGN_RIGHT_MID);

    return row;
}

/***********************************************************
 ****************** 焦点管理 *******************************
 ***********************************************************/

static void __set_focus(int32_t idx, bool focused)
{
    if (idx < 0 || idx >= HOME_MENU_ITEM_COUNT || !sg_menu_items[idx]) return;

    lv_obj_t *item = sg_menu_items[idx];
    if (focused) {
        lv_obj_set_style_bg_color(item, lv_color_black(), 0);
        uint32_t cnt = lv_obj_get_child_count(item);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_set_style_text_color(lv_obj_get_child(item, i), lv_color_white(), 0);
        }
    } else {
        lv_obj_set_style_bg_color(item, lv_color_white(), 0);
        uint32_t cnt = lv_obj_get_child_count(item);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_set_style_text_color(lv_obj_get_child(item, i), lv_color_black(), 0);
        }
        /* 箭头恢复灰色 */
        lv_obj_t *arrow = lv_obj_get_child(item, 2);
        if (arrow) lv_obj_set_style_text_color(arrow, lv_color_make(0x80, 0x80, 0x80), 0);
    }
}

static void __focus_move(int32_t delta)
{
    if (sg_focus_idx >= 0) __set_focus(sg_focus_idx, false);
    sg_focus_idx += delta;
    if (sg_focus_idx >= HOME_MENU_ITEM_COUNT) sg_focus_idx = 0;
    if (sg_focus_idx < 0) sg_focus_idx = HOME_MENU_ITEM_COUNT - 1;
    __set_focus(sg_focus_idx, true);
}

static void __enter_selected(void)
{
    switch (sg_focus_idx) {
    case IDX_CRON:
        display_screen_change(&scr_cron, display_cron_init);
        break;
    case IDX_FOLDER:
        display_screen_change(&scr_folder, display_folder_init);
        break;
    case IDX_NOTIFY:
        display_screen_change(&scr_notify_history, display_notify_history_init);
        break;
    case IDX_SETTING:
        display_screen_change(&scr_setting, display_setting_init);
        break;
    default:
        break;
    }
}

/***********************************************************
 ****************** 触摸点击事件 ***************************
 ***********************************************************/

static void __menu_item_click_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    int32_t idx = (int32_t)(intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        if (sg_focus_idx >= 0) __set_focus(sg_focus_idx, false);
        sg_focus_idx = idx;
        __set_focus(sg_focus_idx, true);
        __enter_selected();
    }
}

/***********************************************************
 ****************** 按键事件 *******************************
 ***********************************************************/

static void __key_event_cb(lv_event_t *e)
{
    lv_key_t key = lv_event_get_key(e);
    switch (key) {
    case LV_KEY_UP:
        __focus_move(-1);
        break;
    case LV_KEY_DOWN:
        __focus_move(1);
        break;
    case LV_KEY_ENTER:
    case LV_KEY_RIGHT:
        __enter_selected();
        break;
    case LV_KEY_ESC:
    case LV_KEY_LEFT:
        /* 返回主页（定时任务页面） */
        display_screen_change(&scr_cron, display_cron_init);
        break;
    default:
        break;
    }
}

static void __screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    sg_group = lv_group_create();
    lv_group_add_obj(sg_group, scr_home);
    lv_indev_t *indev = display_input_get_keypad();
    if (indev) lv_indev_set_group(indev, sg_group);

    for (int i = 0; i < HOME_MENU_ITEM_COUNT; i++) {
        __set_focus(i, false);
    }
    sg_focus_idx = -1;
    __focus_move(1);
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

void display_home_init(void)
{
    if (scr_home) return;

    scr_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_home, lv_color_white(), 0);
    lv_obj_remove_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏 */
    status_bar_create(scr_home, i18n_str(STR_MENU));

    /* 菜单容器 */
    lv_obj_t *menu = lv_obj_create(scr_home);
    lv_obj_set_size(menu, DISP_HOR_RES, CONTENT_HEIGHT);
    lv_obj_set_pos(menu, 0, STATUS_BAR_HEIGHT);
    lv_obj_set_style_pad_all(menu, 0, 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_0, 0);
    lv_obj_set_style_border_width(menu, 0, 0);
    lv_obj_set_style_radius(menu, 0, 0);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 菜单项 */
    sg_menu_items[IDX_CRON]    = __create_menu_item(menu, FONT_AWESOME_BELL,    i18n_str(STR_CRON));
    sg_menu_items[IDX_FOLDER]  = __create_menu_item(menu, FONT_AWESOME_SD_CARD, i18n_str(STR_FOLDER));
    sg_menu_items[IDX_NOTIFY]  = __create_menu_item(menu, FONT_AWESOME_COMMENT, i18n_str(STR_HISTORY));
    sg_menu_items[IDX_SETTING] = __create_menu_item(menu, FONT_AWESOME_GEAR,    i18n_str(STR_SETTING));

    /* 触摸点击事件 */
    for (int i = 0; i < HOME_MENU_ITEM_COUNT; i++) {
        lv_obj_add_event_cb(sg_menu_items[i], __menu_item_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    /* 屏幕事件 */
    lv_obj_add_event_cb(scr_home, __screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 菜单页面初始化完成");
}
