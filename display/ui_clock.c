/**
 * @file ui_clock.c
 * @brief Analog clock UI: tick marks + three hands. Tap anywhere to return home (no swipe / no drag).
 *
 * Entering this screen uses the same LVGL transition as other stacks: duration is
 * DUCKY_SCREEN_TRANS_MS in screen_manager.c (override before including that .c or patch the #define).
 * Hands are positioned immediately; there is no separate pointer "pop" animation in this file.
 */
#include "ui_clock.h"
#include "src/display/lv_display.h"
#include "ui_layout.h"
#include "ai_ui_icon_font.h"
#include "ducky_ui_strings.h"

#include "lvgl.h"
#include "tal_api.h"
#include "tal_time_service.h"

#include <math.h>
#include <string.h>

#ifndef M_PIF
#define M_PIF 3.14159265358979323846f
#endif

enum {
    CLOCK_TICKS = 12,
};

static lv_obj_t *         ui_scr;
static lv_obj_t *         ui_face;
static lv_obj_t *         s_line_ticks[CLOCK_TICKS];
static lv_obj_t *         s_line_hour;
static lv_obj_t *         s_line_min;
static lv_obj_t *         s_line_sec;
static lv_point_precise_t s_pt_ticks[CLOCK_TICKS][2];
static lv_point_precise_t s_pt_hour[2];
static lv_point_precise_t s_pt_min[2];
static lv_point_precise_t s_pt_sec[2];
static lv_timer_t *       s_clock_timer;

static void ui_clock_init(void);
static void ui_clock_deinit(void);

Screen_t ui_clock = {
    .init       = ui_clock_init,
    .deinit     = ui_clock_deinit,
    .screen_obj = &ui_scr,
    .name       = "Clock",
};

/**
 * @brief Any tap closes clock and returns to previous screen
 * @param[in] e LVGL event
 * @return none
 */
static void __ui_clock_tap_back(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    screen_back_anim(LV_SCR_LOAD_ANIM_OUT_LEFT);
}

/**
 * @brief Convert angle in degrees to radians (12 o'clock = up)
 * @param[in] deg_degrees angle in degrees
 * @return radians
 */
static float __deg_to_rad_12up(float deg_degrees)
{
    return (deg_degrees - 90.0f) * (M_PIF / 180.0f);
}

/**
 * @brief Style a line (hand or tick)
 * @param[in] line line object
 * @param[in] width_px width
 * @param[in] c color
 * @return none
 */
static void __style_hand(lv_obj_t *line, int32_t width_px, lv_color_t c)
{
    lv_obj_set_style_line_width(line, width_px, 0);
    lv_obj_set_style_line_color(line, c, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_opa(line, LV_OPA_COVER, 0);
}

/**
 * @brief Build ticks and hands
 * @param[in] face parent
 * @param[in] side square side
 * @return none
 */
static void __clock_face_build(lv_obj_t *face, lv_coord_t side)
{
    int   i;
    float cx = (float)side * 0.5f;
    float cy = (float)side * 0.5f;
    float r_outer;
    float r_inner;
    float a;

    r_outer = (float)side * 0.46f;
    r_inner = (float)side * 0.40f;

    for (i = 0; i < CLOCK_TICKS; i++) {
        a = __deg_to_rad_12up((float)i * 30.0f);
        s_pt_ticks[i][0].x = cx + r_outer * cosf(a);
        s_pt_ticks[i][0].y = cy + r_outer * sinf(a);
        s_pt_ticks[i][1].x = cx + r_inner * cosf(a);
        s_pt_ticks[i][1].y = cy + r_inner * sinf(a);

        s_line_ticks[i] = lv_line_create(face);
        lv_obj_set_size(s_line_ticks[i], side, side);
        lv_obj_set_pos(s_line_ticks[i], 0, 0);
        lv_obj_remove_flag(s_line_ticks[i], LV_OBJ_FLAG_CLICKABLE);
        __style_hand(s_line_ticks[i], 2, lv_color_hex(0xEEEEEE));
        lv_line_set_points(s_line_ticks[i], s_pt_ticks[i], 2);
    }

    s_line_hour = lv_line_create(face);
    s_line_min  = lv_line_create(face);
    s_line_sec  = lv_line_create(face);
    lv_obj_set_size(s_line_hour, side, side);
    lv_obj_set_size(s_line_min, side, side);
    lv_obj_set_size(s_line_sec, side, side);
    lv_obj_set_pos(s_line_hour, 0, 0);
    lv_obj_set_pos(s_line_min, 0, 0);
    lv_obj_set_pos(s_line_sec, 0, 0);
    lv_obj_remove_flag(s_line_hour, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_line_min, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_line_sec, LV_OBJ_FLAG_CLICKABLE);

    __style_hand(s_line_hour, 8, lv_color_hex(0xFFFFFF));
    __style_hand(s_line_min, 6, lv_color_hex(0xFFFFFF));
    __style_hand(s_line_sec, 3, lv_color_hex(0xFF4444));
}

/**
 * @brief Update hands from local time
 * @param[in] side face size
 * @param[in] tm_p local time
 * @return none
 */
static void __hands_from_tm(lv_coord_t side, const POSIX_TM_S *tm_p)
{
    float cx = (float)side * 0.5f;
    float cy = (float)side * 0.5f;
    float rh = (float)side * 0.24f;
    float rm = (float)side * 0.32f;
    float rs = (float)side * 0.38f;
    float ah;
    float am;
    float as;
    int   h12;

    h12 = tm_p->tm_hour % 12;
    if (h12 < 0) {
        h12 += 12;
    }

    ah = __deg_to_rad_12up((float)h12 * 30.0f + (float)tm_p->tm_min * 0.5f);
    am = __deg_to_rad_12up((float)tm_p->tm_min * 6.0f + (float)tm_p->tm_sec * 0.1f);
    as = __deg_to_rad_12up((float)tm_p->tm_sec * 6.0f);

    s_pt_hour[0].x = cx + rh * cosf(ah);
    s_pt_hour[0].y = cy + rh * sinf(ah);
    s_pt_hour[1].x = cx;
    s_pt_hour[1].y = cy;

    s_pt_min[0].x = cx + rm * cosf(am);
    s_pt_min[0].y = cy + rm * sinf(am);
    s_pt_min[1].x = cx;
    s_pt_min[1].y = cy;

    s_pt_sec[0].x = cx + rs * cosf(as);
    s_pt_sec[0].y = cy + rs * sinf(as);
    s_pt_sec[1].x = cx;
    s_pt_sec[1].y = cy;

    lv_line_set_points(s_line_hour, s_pt_hour, 2);
    lv_line_set_points(s_line_min, s_pt_min, 2);
    lv_line_set_points(s_line_sec, s_pt_sec, 2);
}

/**
 * @brief Timer: refresh hands
 * @param[in] t timer
 * @return none
 */
static void __clock_timer_cb(lv_timer_t *t)
{
    TIME_T     now;
    POSIX_TM_S tm;
    lv_coord_t side;

    (void)t;
    if (!ui_face || !s_line_hour) {
        return;
    }

    side = lv_obj_get_width(ui_face);
    if (side <= 0) {
        return;
    }

    now = tal_time_get_posix();
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(now, &tm);
    __hands_from_tm(side, &tm);
}

/**
 * @brief Initialize clock UI
 * @return none
 */
static void ui_clock_init(void)
{
    lv_font_t *f = ai_ui_get_text_font();
    lv_obj_t * panel;
    lv_obj_t * hint;
    lv_obj_t * click_layer;
    lv_coord_t side;

    side = (lv_coord_t)(LV_MIN(LV_HOR_RES, LV_VER_RES) - 2 * DUCKY_ROUND_PAD_Y - 24);
    if (side < 120) {
        side = 120;
    }

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x0a0a12), 0);
    lv_obj_set_style_bg_opa(ui_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ui_scr, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(ui_scr);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_face = lv_obj_create(panel);
    lv_obj_set_size(ui_face, side, side);
    lv_obj_set_style_bg_opa(ui_face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_face, 0, 0);
    lv_obj_set_style_radius(ui_face, 0, 0);
    lv_obj_set_style_pad_all(ui_face, 0, 0);
    lv_obj_remove_flag(ui_face, LV_OBJ_FLAG_SCROLLABLE);

    __clock_face_build(ui_face, side);

    {
        lv_obj_t *hub = lv_obj_create(ui_face);
        lv_coord_t hs = 14;

        lv_obj_set_size(hub, hs, hs);
        lv_obj_center(hub);
        lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(hub, lv_color_hex(0x00C8E8), 0);
        lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hub, 0, 0);
        lv_obj_remove_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    }

    s_clock_timer = lv_timer_create(__clock_timer_cb, 500, NULL);
    __clock_timer_cb(NULL);

    click_layer = lv_obj_create(ui_scr);
    lv_obj_remove_style_all(click_layer);
    lv_obj_set_size(click_layer, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(click_layer, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(click_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(click_layer, 0, 0);
    lv_obj_add_flag(click_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(click_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(click_layer, __ui_clock_tap_back, LV_EVENT_CLICKED, NULL);

    hint = lv_label_create(click_layer);
    lv_label_set_text(hint, ducky_ui_str(DUCKY_UI_STR_CLOCK_HINT));
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    if (f) {
        lv_obj_set_style_text_font(hint, f, 0);
    }
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hint, __ui_clock_tap_back, LV_EVENT_CLICKED, NULL);

    lv_obj_move_foreground(click_layer);
}

/**
 * @brief Deinit clock UI
 * @return none
 */
static void ui_clock_deinit(void)
{
    if (s_clock_timer) {
        lv_timer_del(s_clock_timer);
        s_clock_timer = NULL;
    }
    ui_face     = NULL;
    s_line_hour = NULL;
    s_line_min  = NULL;
    s_line_sec  = NULL;
    memset(s_line_ticks, 0, sizeof(s_line_ticks));
    ui_scr = NULL;
}
