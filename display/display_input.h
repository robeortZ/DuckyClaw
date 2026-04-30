/**
 * @file display_input.h
 * @brief LVGL 按键输入设备 — 将物理按键映射为 LVGL keypad 事件
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_INPUT_H__
#define __DISPLAY_INPUT_H__

#include "tuya_cloud_types.h"
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化按键输入设备并注册为 LVGL keypad
 *
 * 创建 6 个按键（UP/DOWN/LEFT/RIGHT/ENTER/RETURN）的事件处理，
 * 并注册为 LVGL 的 KEYPAD 类型输入设备。
 *
 * @return OPRT_OK 成功
 */
OPERATE_RET display_input_init(void);

/**
 * @brief 获取 LVGL keypad 输入设备句柄
 *
 * @return keypad indev 指针，未初始化时返回 NULL
 */
lv_indev_t *display_input_get_keypad(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_INPUT_H__ */
