/**
 * @file display.h
 * @brief DuckyClaw E-ink 显示 UI 主头文件
 *
 * 提供 UI 初始化接口，包含所有页面和状态栏声明。
 * 显示分辨率: 400x300 (UC8276 E-ink)
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DUCKYCLAW_DISPLAY_H__
#define __DUCKYCLAW_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/***********************************************************
 ************************ 宏定义 ****************************
 ***********************************************************/

/* 显示分辨率 */
#define DISP_HOR_RES 400
#define DISP_VER_RES 300

/* 状态栏高度 */
#define STATUS_BAR_HEIGHT 30

/* 内容区域高度 */
#define CONTENT_HEIGHT (DISP_VER_RES - STATUS_BAR_HEIGHT)

/***********************************************************
 ******************** 字体声明（extern） *********************
 ***********************************************************/

/* 普惠中文字体（来自 ai_components） */
extern lv_font_t font_puhui_14_1;
extern lv_font_t font_puhui_16_2;

/* Font Awesome 图标字体 */
extern lv_font_t font_awesome_14_1;
extern lv_font_t font_awesome_16_4;

/***********************************************************
 ********************* 页面声明 ****************************
 ***********************************************************/

/* 各页面屏幕对象 */
extern lv_obj_t *scr_home;             /* 菜单页面 */
extern lv_obj_t *scr_cron;             /* 定时任务主页（首屏） */
extern lv_obj_t *scr_folder;           /* 文件夹页面 */
extern lv_obj_t *scr_setting;          /* 设置页面 */
extern lv_obj_t *scr_reader;           /* 阅读器页面 */
extern lv_obj_t *scr_notify_history;   /* 历史通知页面 */
extern lv_obj_t *scr_sleep;            /* 休眠页面 */

/***********************************************************
 ******************** 接口函数 *****************************
 ***********************************************************/

/**
 * @brief 初始化 DuckyClaw 显示系统
 */
OPERATE_RET display_init(void);

/**
 * @brief 页面切换辅助函数
 */
void display_screen_change(lv_obj_t **target, void (*init_fn)(void));

/**
 * @brief 获取当前 WiFi 状态图标字符串
 */
const char *display_get_wifi_icon(void);

/**
 * @brief 获取当前电池状态图标字符串（根据实际电量和充电状态）
 */
const char *display_get_battery_icon(void);

/**
 * @brief 更新 WiFi 状态
 */
void display_set_wifi_status(uint8_t status);

#ifdef __cplusplus
}
#endif

#endif /* __DUCKYCLAW_DISPLAY_H__ */
