/**
 * @file claw_wallpaper_prefs.h
 * @brief Persisted preference: shake-to-cycle SD wallpapers on home (default off)
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef CLAW_WALLPAPER_PREFS_H
#define CLAW_WALLPAPER_PREFS_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read shake-wallpaper enable (0=off, default builtin only)
 * @return 1 if enabled, 0 if disabled or missing key
 */
uint8_t claw_wallpaper_prefs_shake_enabled_get(void);

/**
 * @brief Save shake-wallpaper enable
 * @param[in] enable 0 or 1
 * @return OPRT_OK on success
 */
OPERATE_RET claw_wallpaper_prefs_shake_enabled_set(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* CLAW_WALLPAPER_PREFS_H */
