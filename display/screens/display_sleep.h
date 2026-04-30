/**
 * @file display_sleep.h
 * @brief 休眠页面与深度休眠管理器
 *
 * 提供：
 *   - 休眠显示页面 (scr_sleep)
 *   - 空闲定时器管理器（自动触发深度休眠）
 *   - 按键活动检测（重置定时器）
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_SLEEP_H__
#define __DISPLAY_SLEEP_H__

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化休眠页面 LVGL screen
 *
 * 创建 400x300 全白休眠页面，显示：
 *   "AI" (左上)
 *   "T5 Eink Note" (中央大字)
 *   "build by DuckyClaw" (中央小字)
 *   "设备已经休眠，按任意按键唤醒" (底部中央)
 */
void display_sleep_init(void);

/**
 * @brief 初始化深度休眠管理器
 *
 * 启动空闲定时器，默认超时时间 5 分钟
 * 任何按键活动都会重置定时器
 */
void sleep_manager_init(void);

/**
 * @brief 设置休眠超时时间
 *
 * @param minutes 超时分钟数 (1/3/5/15/30)
 */
void sleep_manager_set_timeout(uint32_t minutes);

/**
 * @brief 报告用户活动（按键）— 重置定时器
 *
 * 在每次按键事件后调用，重新开始计时
 */
void sleep_manager_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_SLEEP_H__ */
