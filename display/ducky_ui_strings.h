/**
 * @file ducky_ui_strings.h
 * @brief Runtime UI strings (English / Chinese), preference via settings + KV
 * @version 1.0
 * @date 2026-04-01
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __DUCKY_UI_STRINGS_H__
#define __DUCKY_UI_STRINGS_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------------- */
/**
 * @brief String table indices (keep in sync with ducky_ui_strings.c arrays)
 */
typedef enum {
    DUCKY_UI_STR_MENU = 0,
    DUCKY_UI_STR_MENU_TODO,
    DUCKY_UI_STR_MENU_CRON,
    DUCKY_UI_STR_MENU_MUSIC,
    DUCKY_UI_STR_MENU_SETTINGS,
    DUCKY_UI_STR_SETTINGS_TITLE,
    DUCKY_UI_STR_BRIGHTNESS,
    DUCKY_UI_STR_VOLUME,
    DUCKY_UI_STR_SHAKE_WALLPAPER,
    DUCKY_UI_STR_SETTINGS_LANG,
    DUCKY_UI_STR_RESET_NETCFG,
    DUCKY_UI_STR_HINT_BACK_LEVEL,
    DUCKY_UI_STR_NOTICE_POPUP,
    DUCKY_UI_STR_NOTICE_LIST_TITLE,
    DUCKY_UI_STR_NOTICE_EMPTY,
    DUCKY_UI_STR_NOTICE_HINT,
    DUCKY_UI_STR_CLOCK_HINT,
    DUCKY_UI_STR_TODO_EMPTY,
    DUCKY_UI_STR_TODO_TITLE,
    DUCKY_UI_STR_CRON_EMPTY,
    DUCKY_UI_STR_CRON_TITLE,
    DUCKY_UI_STR_MUSIC_IDLE_TITLE,
    DUCKY_UI_STR_MUSIC_IDLE_ARTIST,
    DUCKY_UI_STR_MUSIC_AI_DISABLED,
    DUCKY_UI_STR_MUSIC_CMD_PREV,
    DUCKY_UI_STR_MUSIC_CMD_NEXT,
    DUCKY_UI_STR_MUSIC_CMD_PLAY,
    DUCKY_UI_STR_COUNT
} ducky_ui_string_id_e;

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Load UI language from KV (call once before UI uses strings)
 * @return none
 */
void ducky_ui_lang_init(void);

/**
 * @brief Whether UI uses English (1) or Chinese (0)
 * @return 1 English, 0 Chinese
 */
uint8_t ducky_ui_lang_en_get(void);

/**
 * @brief Set UI language and persist
 * @param[in] en 1 English, 0 Chinese
 * @return OPRT_OK on success
 */
OPERATE_RET ducky_ui_lang_en_set(uint8_t en);

/**
 * @brief Localized string for current language
 * @param[in] id string id
 * @return literal C string, empty if id invalid
 */
const char *ducky_ui_str(ducky_ui_string_id_e id);

#ifdef __cplusplus
}
#endif

#endif /* __DUCKY_UI_STRINGS_H__ */
