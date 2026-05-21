/**
 * @file claw_wallpaper_prefs.c
 * @brief KV storage for home wallpaper shake mode
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "claw_wallpaper_prefs.h"

#include "tal_api.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define CLAW_KV_SHAKE_WALLPAPER "claw_shk_wp"

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Read shake-wallpaper enable (0=off, default builtin only)
 * @return 1 if enabled, 0 if disabled or missing key
 */
uint8_t claw_wallpaper_prefs_shake_enabled_get(void)
{
    uint8_t *buf  = NULL;
    size_t   len  = 0;
    uint8_t  out  = 0;

    if (tal_kv_get(CLAW_KV_SHAKE_WALLPAPER, &buf, &len) != OPRT_OK || buf == NULL || len < 1) {
        if (buf) {
            tal_kv_free(buf);
        }
        return 0;
    }
    out = buf[0] ? 1 : 0;
    tal_kv_free(buf);
    return out;
}

/**
 * @brief Save shake-wallpaper enable
 * @param[in] enable 0 or 1
 * @return OPRT_OK on success
 */
OPERATE_RET claw_wallpaper_prefs_shake_enabled_set(uint8_t enable)
{
    uint8_t v = enable ? 1 : 0;
    return tal_kv_set(CLAW_KV_SHAKE_WALLPAPER, &v, 1);
}
