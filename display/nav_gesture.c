/**
 * @file nav_gesture.c
 * @brief Swipe navigation implementation
 *
 * From home (is_home=1):
 * - LV_DIR_LEFT -> menu
 * - LV_DIR_RIGHT -> full-screen analog clock (ui_clock)
 * - LV_DIR_BOTTOM -> recent cloud notices (screen_notice_list)
 *
 * From other screens (is_home=0):
 * - LV_DIR_TOP or LV_DIR_RIGHT -> screen_back() (menu or home)
 * - Swipe opposite(entry_from_home) -> screen_back() (e.g. music entered from home right)
 *
 * Menu: up or right pops to home. Sub-screens opened from menu: up/right pops to menu.
 *
 * Uses lv_async_call so navigation runs outside nested lv_display lock
 * (gesture is processed while the GUI mutex may already be held).
 */
#include "nav_gesture.h"

#include "lvgl.h"

#include "screen_manager.h"
#include "screen_menu.h"
#include "screen_notice_list.h"
#include "ducky_notice_unread.h"
#include "startup_screen.h"
#include "src/display/lv_display.h"
#include "ui_clock.h"

#include <stdint.h>

static lv_dir_t __nav_opposite(lv_dir_t d)
{
    if (d == LV_DIR_LEFT) {
        return LV_DIR_RIGHT;
    }
    if (d == LV_DIR_RIGHT) {
        return LV_DIR_LEFT;
    }
    if (d == LV_DIR_TOP) {
        return LV_DIR_BOTTOM;
    }
    if (d == LV_DIR_BOTTOM) {
        return LV_DIR_TOP;
    }
    return LV_DIR_NONE;
}

/**
 * @brief Pack gesture + screen role into a single pointer-sized value for lv_async_call (no heap).
 */
static void *__nav_pack_ud(int is_home, lv_dir_t dir, lv_dir_t entry_dir)
{
    uintptr_t v;

    v = (uintptr_t)(is_home ? 1u : 0u);
    v |= ((uintptr_t)(uint8_t)dir << 8);
    v |= ((uintptr_t)(uint8_t)entry_dir << 16);
    return (void *)v;
}

static void __nav_async_cb(void *user_data)
{
    uintptr_t p         = (uintptr_t)user_data;
    int       is_home   = (int)(p & 1u);
    lv_dir_t  dir       = (lv_dir_t)((p >> 8) & 0xffu);
    lv_dir_t  entry_dir = (lv_dir_t)((p >> 16) & 0xffu);

    if (is_home) {
        if (dir == LV_DIR_LEFT) {
            screen_load(&screen_menu);
        } else if (dir == LV_DIR_RIGHT) {
            screen_load_anim(&ui_clock, LV_SCR_LOAD_ANIM_OUT_RIGHT);
        } else if (dir == LV_DIR_BOTTOM) {
            ducky_notice_unread_clear();
            startup_screen_notice_badge_refresh();
            screen_load_anim(&screen_notice_list, LV_SCR_LOAD_ANIM_OVER_BOTTOM);
        }
    } else {
        if (dir == LV_DIR_TOP || dir == LV_DIR_RIGHT) {
            screen_back();
        } else {
            lv_dir_t opp = __nav_opposite(entry_dir);
            if (opp != LV_DIR_NONE && dir == opp) {
                screen_back();
            }
        }
    }
}

static void __nav_gesture_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(e);
    if (!indev) {
        indev = lv_indev_active();
    }
    if (!indev) {
        return;
    }

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    uint32_t ud  = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    if (lv_async_call(__nav_async_cb, __nav_pack_ud((int)(ud & 1u), dir, (lv_dir_t)((ud >> 8) & 0xffu))) !=
        LV_RESULT_OK) {
        /* Drop gesture if async queue is full */
    }
}

void nav_gesture_attach(lv_obj_t *screen_root, int is_home, lv_dir_t entry_from_home)
{
    uint32_t ud;

    if (!screen_root) {
        return;
    }
    ud = (is_home ? 1u : 0u) | (((uint32_t)entry_from_home) << 8);
    lv_obj_add_event_cb(screen_root, __nav_gesture_event, LV_EVENT_GESTURE, (void *)(uintptr_t)ud);
    lv_obj_remove_flag(screen_root, LV_OBJ_FLAG_GESTURE_BUBBLE);
}
