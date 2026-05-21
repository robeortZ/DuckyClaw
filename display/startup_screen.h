/**
 * @file startup_screen.h
 * @brief Declaration of the startup screen for the application
 *
 * This file contains the declarations for the startup screen which is displayed
 * when the application starts. It shows a splash screen with "TuyaOpen" and
 * "AI Pocket Pet Demo" text, and automatically transitions after a timeout.
 *
 * The startup screen includes:
 * - Screen initialization and deinitialization functions
 * - Screen structure definition for the screen manager
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef STARTUP_SCREEN_H
#define STARTUP_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t startup_screen;

/**
 * @brief Re-apply home background after settings change (builtin bg2 image).
 * @note Call from LVGL context only.
 * @return none
 */
void startup_screen_apply_wallpaper_prefs(void);

/**
 * @brief Update bell + unread badge on home (LVGL thread only)
 * @return none
 */
void startup_screen_notice_badge_refresh(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*STARTUP_SCREEN_H*/
