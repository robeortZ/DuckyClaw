/**
 * @file nav_gesture.h
 * @brief Home: left -> menu, right -> analog clock. Sub: top/right (or opposite of entry) -> screen_back().
 */
#ifndef NAV_GESTURE_H
#define NAV_GESTURE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Attach gesture handler to a full-screen root object.
 * @param is_home true for home screen
 * @param entry_from_home Direction from home (e.g. LV_DIR_LEFT for menu); LV_DIR_NONE if opened from menu
 */
void nav_gesture_attach(lv_obj_t *screen_root, int is_home, lv_dir_t entry_from_home);

#ifdef __cplusplus
}
#endif

#endif /* NAV_GESTURE_H */
