/**
 * @file display_cron.h
 * @brief 定时任务列表页面
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_CRON_H__
#define __DISPLAY_CRON_H__

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化定时任务页面
 */
void display_cron_init(void);

/**
 * @brief 刷新定时任务列表数据
 */
void display_cron_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_CRON_H__ */
