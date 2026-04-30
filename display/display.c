/**
 * @file display.c
 * @brief DuckyClaw E-ink 显示系统初始化
 *
 * 负责 LVGL 初始化、AI UI 回调注册、页面创建和按键输入绑定。
 * 首屏为定时任务主页（scr_cron）。
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display.h"
#include "display_status_bar.h"
#include "display_input.h"
#include "screens/display_home.h"
#include "screens/display_cron.h"
#include "screens/display_folder.h"
#include "screens/display_setting.h"
#include "screens/display_reader.h"
#include "screens/display_notify.h"
#include "screens/display_sleep.h"

#include "tal_log.h"
#include "lv_vendor.h"
#include "ai_ui_manage.h"
#include "font_awesome_symbols.h"
#include "board_charge_detect_api.h"

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/

/* WiFi 状态 */
static uint8_t sg_wifi_status = 0;

/***********************************************************
 ******************* AI UI 回调实现 ************************
 ***********************************************************/

static OPERATE_RET __ai_ui_disp_init(void)
{
    PR_NOTICE("[display] AI UI 显示初始化 (%dx%d)", DISP_HOR_RES, DISP_VER_RES);

    /* 1. 初始化 LVGL 并启动渲染任务 */
    lv_vendor_init(DISPLAY_NAME);
    lv_vendor_start(5, 1024 * 8);

    /* 1.1 关闭背光（墨水屏不需要背光） */
    lv_vendor_set_backlight(0);

    /* 2. 注册按键输入设备 */
    display_input_init();

    /* 3. 创建首屏 — 定时任务主页 */
    lv_vendor_disp_lock();

    display_cron_init();
    lv_disp_load_scr(scr_cron);

    /* 4. 启动状态栏定时刷新 */
    status_bar_timer_start();

    lv_vendor_disp_unlock();

    /* 5. 初始化深度休眠管理器 */
    sleep_manager_init();

    PR_NOTICE("[display] 显示系统初始化完成");
    return OPRT_OK;
}

static void __ai_ui_disp_wifi_state(uint8_t wifi_status)
{
    sg_wifi_status = wifi_status;
    status_bar_refresh();
}

static void __ai_ui_disp_stream_start(void) {}
static void __ai_ui_disp_stream_data(char *string) { (void)string; }
static void __ai_ui_disp_stream_end(void) {}

static AI_UI_INTFS_T sg_ui_intfs = {
    .disp_init                = __ai_ui_disp_init,
    .disp_wifi_state          = __ai_ui_disp_wifi_state,
    .disp_ai_msg_stream_start = __ai_ui_disp_stream_start,
    .disp_ai_msg_stream_data  = __ai_ui_disp_stream_data,
    .disp_ai_msg_stream_end   = __ai_ui_disp_stream_end,
};

/***********************************************************
 ********************* 公共函数 ****************************
 ***********************************************************/

void display_screen_change(lv_obj_t **target, void (*init_fn)(void))
{
    if (*target == NULL) {
        init_fn();
    }
    lv_screen_load_anim(*target, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, false);
}

const char *display_get_wifi_icon(void)
{
    switch (sg_wifi_status) {
    case 1: return FONT_AWESOME_WIFI;
    case 2: return FONT_AWESOME_WIFI_FAIR;
    case 3: return FONT_AWESOME_WIFI_WEAK;
    default: return FONT_AWESOME_WIFI_OFF;
    }
}

const char *display_get_battery_icon(void)
{
    BOARD_CHARGE_STATE_E charge_state = BOARD_CHARGE_STATE_UNPLUGGED;
    uint8_t percentage = 0;

    /* 检测充电状态 */
    if (OPRT_OK == board_charge_detect_get_state(&charge_state) &&
        charge_state == BOARD_CHARGE_STATE_PLUGGED) {
        return FONT_AWESOME_BATTERY_CHARGING;
    }

    /* 读取电量百分比 */
    if (OPRT_OK != board_battery_read_percentage(&percentage)) {
        return FONT_AWESOME_BATTERY_SLASH;
    }

    if (percentage >= 75) return FONT_AWESOME_BATTERY_FULL;
    if (percentage >= 50) return FONT_AWESOME_BATTERY_3;
    if (percentage >= 25) return FONT_AWESOME_BATTERY_2;
    if (percentage >= 10) return FONT_AWESOME_BATTERY_1;
    return FONT_AWESOME_BATTERY_EMPTY;
}

void display_set_wifi_status(uint8_t status)
{
    sg_wifi_status = status;
}

/***********************************************************
 ******************** 初始化入口 ***************************
 ***********************************************************/

OPERATE_RET display_init(void)
{
    PR_NOTICE("[display] 注册自定义 AI UI 回调");
    ai_ui_register(&sg_ui_intfs);
    return OPRT_OK;
}
