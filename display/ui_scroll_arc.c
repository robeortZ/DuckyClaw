/**
 * @file ui_scroll_arc.c
 * @brief Arc-style vertical scroll (ported from lv_example_scroll_6)
 */
#include "ui_scroll_arc.h"

#include "lvgl.h"

void ducky_scroll_arc_event_cb(lv_event_t *e)
{
    lv_obj_t *        cont = lv_event_get_target(e);
    lv_area_t         cont_a;
    int32_t           cont_y_center;
    int32_t           r;
    uint32_t          i;
    uint32_t          child_cnt;
    lv_event_code_t   code = lv_event_get_code(e);

    if (code != LV_EVENT_SCROLL) {
        return;
    }

    lv_obj_get_coords(cont, &cont_a);
    cont_y_center = cont_a.y1 + lv_area_get_height(&cont_a) / 2;
    r = lv_obj_get_height(cont) * 7 / 10;
    if (r < 8) {
        r = 8;
    }

    child_cnt = lv_obj_get_child_count(cont);
    for (i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t  child_a;
        int32_t    child_y_center;
        int32_t    diff_y;
        int32_t    x;
        lv_opa_t   opa;

        lv_obj_get_coords(child, &child_a);
        child_y_center = child_a.y1 + lv_area_get_height(&child_a) / 2;
        diff_y = child_y_center - cont_y_center;
        diff_y = LV_ABS(diff_y);

        if (diff_y >= r) {
            x = r;
        } else {
            uint32_t        x_sqr = (uint32_t)(r * r - diff_y * diff_y);
            lv_sqrt_res_t   res;
            lv_sqrt(x_sqr, &res, 0x8000);
            x = r - res.i;
        }

        lv_obj_set_style_translate_x(child, x, 0);

        opa = lv_map(x, 0, r, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_obj_set_style_opa(child, LV_OPA_COVER - opa, 0);
    }
}

void ducky_scroll_arc_setup(lv_obj_t *cont)
{
    if (!cont) {
        return;
    }

    lv_obj_add_event_cb(cont, ducky_scroll_arc_event_cb, LV_EVENT_SCROLL, NULL);
    /* Snap would pull scroll to fixed anchors after release; keep finger end position. */
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_NONE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
}

void ducky_scroll_arc_refresh(lv_obj_t *cont, int32_t restore_scroll_y)
{
    if (!cont) {
        return;
    }
    lv_obj_update_layout(cont);
    lv_obj_scroll_to_y(cont, restore_scroll_y, LV_ANIM_OFF);
    lv_obj_send_event(cont, LV_EVENT_SCROLL, NULL);
}

void ducky_scroll_arc_h_event_cb(lv_event_t *e)
{
    lv_obj_t *        cont = lv_event_get_target(e);
    lv_area_t         cont_a;
    int32_t           cont_x_center;
    int32_t           r;
    uint32_t          i;
    uint32_t          child_cnt;

    if (lv_event_get_code(e) != LV_EVENT_SCROLL) {
        return;
    }

    lv_obj_get_coords(cont, &cont_a);
    cont_x_center = cont_a.x1 + lv_area_get_width(&cont_a) / 2;
    r = lv_obj_get_width(cont) * 7 / 10;
    if (r < 8) {
        r = 8;
    }

    child_cnt = lv_obj_get_child_count(cont);
    for (i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t  child_a;
        int32_t    child_x_center;
        int32_t    diff_x;
        int32_t    y_off;
        lv_opa_t   opa;

        lv_obj_get_coords(child, &child_a);
        child_x_center = child_a.x1 + lv_area_get_width(&child_a) / 2;
        diff_x = child_x_center - cont_x_center;
        diff_x = LV_ABS(diff_x);

        if (diff_x >= r) {
            y_off = r;
        } else {
            uint32_t        y_sqr = (uint32_t)(r * r - diff_x * diff_x);
            lv_sqrt_res_t   res;
            lv_sqrt(y_sqr, &res, 0x8000);
            y_off = r - res.i;
        }

        lv_obj_set_style_translate_y(child, y_off, 0);
        opa = lv_map(y_off, 0, r, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_obj_set_style_opa(child, LV_OPA_COVER - opa, 0);
    }
}

void ducky_scroll_arc_h_setup(lv_obj_t *cont)
{
    if (!cont) {
        return;
    }

    lv_obj_add_event_cb(cont, ducky_scroll_arc_h_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_set_scroll_snap_x(cont, LV_SCROLL_SNAP_NONE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
}

void ducky_scroll_arc_h_refresh(lv_obj_t *cont, int32_t restore_scroll_x)
{
    if (!cont) {
        return;
    }
    lv_obj_update_layout(cont);
    lv_obj_scroll_to_x(cont, restore_scroll_x, LV_ANIM_OFF);
    lv_obj_send_event(cont, LV_EVENT_SCROLL, NULL);
}

/* Center = LV_SCALE_NONE (256); off-center tiles shrink (Cover Flow / iPod-style) */
#define DUCKY_H_BALLOON_SCALE_MIN 200

void ducky_scroll_arc_h_balloon_event_cb(lv_event_t *e)
{
    lv_obj_t *        cont = lv_event_get_target(e);
    lv_area_t         cont_a;
    int32_t           cont_x_center;
    int32_t           r;
    uint32_t          i;
    uint32_t          child_cnt;
    int32_t           scale_val;

    if (lv_event_get_code(e) != LV_EVENT_SCROLL) {
        return;
    }

    lv_obj_get_coords(cont, &cont_a);
    cont_x_center = cont_a.x1 + lv_area_get_width(&cont_a) / 2;
    r = lv_obj_get_width(cont) * 7 / 10;
    if (r < 8) {
        r = 8;
    }

    child_cnt = lv_obj_get_child_count(cont);
    for (i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        lv_area_t  child_a;
        int32_t    child_x_center;
        int32_t    diff_x;

        lv_obj_get_coords(child, &child_a);
        child_x_center = child_a.x1 + lv_area_get_width(&child_a) / 2;
        diff_x = child_x_center - cont_x_center;
        diff_x = LV_ABS(diff_x);

        if (diff_x >= r) {
            scale_val = DUCKY_H_BALLOON_SCALE_MIN;
        } else {
            scale_val = lv_map(diff_x, 0, r, (int32_t)LV_SCALE_NONE, DUCKY_H_BALLOON_SCALE_MIN);
        }

        lv_obj_set_style_transform_scale(child, scale_val, 0);
        lv_obj_set_style_translate_y(child, 0, 0);
        lv_obj_set_style_opa(child, LV_OPA_COVER, 0);
    }
}

void ducky_scroll_arc_h_balloon_setup(lv_obj_t *cont)
{
    if (!cont) {
        return;
    }

    lv_obj_add_event_cb(cont, ducky_scroll_arc_h_balloon_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_set_scroll_snap_x(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);
}

void ducky_scroll_arc_h_balloon_refresh(lv_obj_t *cont, int32_t restore_scroll_x)
{
    ducky_scroll_arc_h_refresh(cont, restore_scroll_x);
}
