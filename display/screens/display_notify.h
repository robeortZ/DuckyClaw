/**
 * @file display_notify.h
 * @brief 通知弹窗 + 历史通知存储与浏览
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_NOTIFY_H__
#define __DISPLAY_NOTIFY_H__

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NOTIFY_MAX_HISTORY   20   /* 最多保存的历史通知数 */
#define NOTIFY_TITLE_MAX     64   /* 单条通知标题最大长度 */
#define NOTIFY_CONTENT_MAX   256  /* 单条通知内容最大长度 */

/* 历史通知页面屏幕对象 */
extern lv_obj_t *scr_notify_history;

/**
 * @brief 弹出通知（全屏，任意键/触摸关闭）
 *
 * 会同时保存到历史记录并播放提示音。
 * 可从任意线程调用。
 *
 * @param title   通知标题（如 "温度异常报警"）
 * @param content 通知内容
 */
void display_notify_show(const char *title, const char *content);

/**
 * @brief 初始化历史通知浏览页面
 */
void display_notify_history_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_NOTIFY_H__ */
