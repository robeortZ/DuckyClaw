/**
 * @file display_status_bar.h
 * @brief 状态栏组件 — 显示时间、页面标题、WiFi和电池状态
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_STATUS_BAR_H__
#define __DISPLAY_STATUS_BAR_H__

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在指定屏幕顶部创建状态栏
 *
 * @param parent 父屏幕对象
 * @param title  页面标题（如 "主页"、"设置" 等）
 * @return 状态栏容器对象
 */
lv_obj_t *status_bar_create(lv_obj_t *parent, const char *title);

/**
 * @brief 刷新状态栏（时间、WiFi等）
 */
void status_bar_refresh(void);

/**
 * @brief 更新指定状态栏实例的标题
 *
 * @param bar 状态栏容器对象
 * @param title 新标题文本
 */
void status_bar_set_title(lv_obj_t *bar, const char *title);

/**
 * @brief 重置状态栏跟踪（当所有页面被销毁时调用，避免访问已释放对象）
 */
void status_bar_reset(void);

/**
 * @brief 启动状态栏定时刷新（每30秒）
 */
void status_bar_timer_start(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_STATUS_BAR_H__ */
