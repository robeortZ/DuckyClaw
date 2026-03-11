/**
 * @file button_manege.h
 * @brief Button management for volume keys (BUTTON_NAME_2 / BUTTON_NAME_3)
 *
 * Single click: BUTTON_NAME_2 = volume up, BUTTON_NAME_3 = volume down.
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUTTON_MANEGE_H__
#define __BUTTON_MANEGE_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize volume buttons (BUTTON_NAME_2 / BUTTON_NAME_3).
 *
 * Call after board_register_hardware() and ai_chat (e.g. ducky_claw_chat_init).
 * No-op if neither BUTTON_NAME_2 nor BUTTON_NAME_3 is defined.
 *
 * @return OPERATE_RET OPRT_OK on success
 */
OPERATE_RET voice_manager_buttons_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_MANEGE_H__ */
