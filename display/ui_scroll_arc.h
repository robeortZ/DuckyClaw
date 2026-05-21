/**
 * @file ui_scroll_arc.h
 * @brief Vertical list scroll effect (LVGL lv_example_scroll_6 style)
 */
#ifndef UI_SCROLL_ARC_H
#define UI_SCROLL_ARC_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LV_EVENT_SCROLL: translate-x + opacity by distance from viewport Y center (arc feel).
 */
void ducky_scroll_arc_event_cb(lv_event_t *e);

/**
 * @brief Register scroll effect (no Y snap / elastic so offset stays after release).
 * @param cont Scrollable column container (direct children are animated).
 */
void ducky_scroll_arc_setup(lv_obj_t *cont);

/**
 * @brief After lv_obj_clean + rebuild: layout, restore vertical scroll, arc repaint.
 * @param restore_scroll_y Value from lv_obj_get_scroll_y() before clean (lv_obj_scroll_to_y clamps).
 */
void ducky_scroll_arc_refresh(lv_obj_t *cont, int32_t restore_scroll_y);

/**
 * @brief Horizontal scroll: translate-y + opacity from distance to viewport X center (scroll_6 style).
 */
void ducky_scroll_arc_h_event_cb(lv_event_t *e);

void ducky_scroll_arc_h_setup(lv_obj_t *cont);

void ducky_scroll_arc_h_refresh(lv_obj_t *cont, int32_t restore_scroll_x);

/**
 * @brief Horizontal scroll: transform_scale by |x - viewport center| (center largest, sides smaller).
 * @note Set transform pivot to each child's center after layout; parent may use LV_OBJ_FLAG_OVERFLOW_VISIBLE.
 */
void ducky_scroll_arc_h_balloon_event_cb(lv_event_t *e);

void ducky_scroll_arc_h_balloon_setup(lv_obj_t *cont);

void ducky_scroll_arc_h_balloon_refresh(lv_obj_t *cont, int32_t restore_scroll_x);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCROLL_ARC_H */
