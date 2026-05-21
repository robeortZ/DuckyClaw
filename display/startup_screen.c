/**
 * @file startup_screen.c
 * @brief Home screen: builtin bg_3 wallpaper. Clock, Wi-Fi, swipe nav.
 *        Swipe right opens analog clock (ui_clock); swipe down opens recent notices; tap on clock returns home.
 */
#include "startup_screen.h"
#include "src/core/lv_obj_pos.h"
#include "wallpaper_jpeg.h"
#include "nav_gesture.h"
#include "ducky_notice_unread.h"
#include "screen_notice_list.h"
#include "ai_ui_icon_font.h"
#include "font_awesome_symbols.h"

#include "lvgl.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_time_service.h"

#include <stdio.h>
#include <string.h>

#ifndef LV_SYMBOL_WIFI
#define LV_SYMBOL_WIFI "\xEF\x87\xAB"
#endif

#define DUCKY_NOTICE_HIT_SIDE   96
#define DUCKY_NOTICE_BELL_BLUE  0x2196F3
#define DUCKY_NOTICE_BADGE_RED   0xFF3B30

extern const lv_font_t my_font_46;
extern const lv_image_dsc_t bg_3;

static lv_obj_t *       ui_startup_screen;
static lv_obj_t *       ui_bg_image;
static lv_obj_t *       ui_clock_row;
static lv_obj_t *       ui_clock_hh;
static lv_obj_t *       ui_clock_colon;
static lv_obj_t *       ui_clock_mm;
static lv_obj_t *       ui_wifi_label;
static lv_timer_t *     sg_clock_timer;
static lv_timer_t *     sg_shake_timer;
static wallpaper_list_t sg_wall_list;
static lv_image_dsc_t   sg_wall_dsc;
static uint8_t          sg_clock_colon_on = 1;
static uint8_t          sg_clock_hhmm_cache_valid;
static lv_obj_t *       ui_notice_hit;
static lv_obj_t *       ui_notice_badge_lbl;

static void startup_screen_init(void);
static void startup_screen_deinit(void);
static void __notice_badge_apply(void);
static void __on_notice_bell_clicked(lv_event_t *e);

/**
 * @brief Font for bell glyph (Font Awesome); fallback to UI text / default
 * @return Font pointer
 */
static const lv_font_t *__notice_bell_font(void)
{
    lv_font_t *f;

    f = ai_ui_get_icon_font();
    if (f) {
        return f;
    }
    f = ai_ui_get_text_font();
    if (f) {
        return f;
    }
    return lv_font_default();
}

Screen_t startup_screen = {
    .init = startup_screen_init,
    .deinit = startup_screen_deinit,
    .screen_obj = &ui_startup_screen,
    .name = "Startup",
};

static void __style_clock_hh_mm(lv_obj_t *lbl)
{
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(lbl, &my_font_46, 0);
}

static void __style_clock_colon(lv_obj_t *lbl)
{
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(lbl, &my_font_46, 0);
}

static void __update_clock_label(void)
{
    char           hb[4];
    char           mb[4];
    TIME_T         now;
    POSIX_TM_S     tm;
    static char s_prev_hh[4];
    static char s_prev_mm[4];

    if (!ui_clock_hh || !ui_clock_colon || !ui_clock_mm) {
        return;
    }

    now = tal_time_get_posix();
    memset(&tm, 0, sizeof(tm));
    tal_time_get_local_time_custom(now, &tm);

    snprintf(hb, sizeof(hb), "%02d", tm.tm_hour);
    snprintf(mb, sizeof(mb), "%02d", tm.tm_min);
    if (!sg_clock_hhmm_cache_valid || strcmp(hb, s_prev_hh) != 0) {
        lv_label_set_text(ui_clock_hh, hb);
        memcpy(s_prev_hh, hb, sizeof(hb));
    }
    if (!sg_clock_hhmm_cache_valid || strcmp(mb, s_prev_mm) != 0) {
        lv_label_set_text(ui_clock_mm, mb);
        memcpy(s_prev_mm, mb, sizeof(mb));
    }
    sg_clock_hhmm_cache_valid = 1;

    lv_obj_set_style_text_opa(ui_clock_colon, sg_clock_colon_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    sg_clock_colon_on = (uint8_t)(sg_clock_colon_on ? 0 : 1);
}

/**
 * @brief Apply builtin home background (display/image/bg_3.c)
 * @return none
 */
static void __apply_home_builtin_bg(void)
{
    int32_t scale;

    wallpaper_free_decoded(&sg_wall_dsc);
    memset(&sg_wall_dsc, 0, sizeof(sg_wall_dsc));
    if (!ui_bg_image) {
        return;
    }

    lv_image_set_src(ui_bg_image, &bg_3);
    lv_obj_set_size(ui_bg_image, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui_bg_image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_transform_pivot_x(ui_bg_image, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(ui_bg_image, LV_PCT(50), 0);

    scale = (int32_t)((LV_VER_RES * 256) / (lv_coord_t)bg_3.header.h);
    if (bg_3.header.w > 0) {
        lv_coord_t hor_res = (lv_coord_t)LV_HOR_RES;
        lv_coord_t scaled_w = (lv_coord_t)(((int32_t)bg_3.header.w * scale) / 256);

        if (scaled_w < hor_res) {
            scale = (int32_t)((hor_res * 256) / (lv_coord_t)bg_3.header.w);
        }
    }
    lv_obj_set_style_transform_scale(ui_bg_image, scale, 0);
    lv_obj_remove_flag(ui_bg_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(ui_bg_image);
}

static void __clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    __update_clock_label();
}

/**
 * @brief Shake timer stub: SD wallpaper cycling disabled; keep builtin bg_3
 * @param[in] t LVGL timer
 * @return none
 */
static void __shake_timer_cb(lv_timer_t *t)
{
    (void)t;
}

/**
 * @brief Refresh notification bell visibility and red count (LVGL thread)
 * @return none
 */
void startup_screen_notice_badge_refresh(void)
{
    __notice_badge_apply();
}

/**
 * @brief Apply unread count to bell badge; hide entire control when zero
 * @return none
 */
static void __notice_badge_apply(void)
{
    int                n;
    char               buf[8];
    lv_obj_t *         bell;
    const lv_font_t *  sym_font;

    if (!ui_notice_hit || !ui_notice_badge_lbl) {
        return;
    }
    if (!lv_obj_is_valid(ui_notice_hit)) {
        return;
    }

    n = ducky_notice_unread_get();
    if (n <= 0) {
        lv_obj_add_flag(ui_notice_hit, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(ui_notice_hit, LV_OBJ_FLAG_HIDDEN);
    if (n > 99) {
        (void)snprintf(buf, sizeof(buf), "99+");
    } else {
        (void)snprintf(buf, sizeof(buf), "%d", n);
    }
    lv_label_set_text(ui_notice_badge_lbl, buf);
    {
        lv_obj_t *dot = lv_obj_get_parent(ui_notice_badge_lbl);
        if (dot) {
            lv_obj_update_layout(dot);
        }
    }

    bell = lv_obj_get_child(ui_notice_hit, 0);
    if (bell && lv_obj_check_type(bell, &lv_label_class)) {
        sym_font = __notice_bell_font();
        if (sym_font) {
            lv_obj_set_style_text_font(bell, sym_font, 0);
        }
    }
}

static void __on_notice_bell_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ducky_notice_unread_clear();
    __notice_badge_apply();
    screen_load_anim(&screen_notice_list, LV_SCR_LOAD_ANIM_OVER_BOTTOM);
}

/**
 * @brief Re-apply home background after settings change (builtin bg_3)
 * @note LVGL thread only; shake timer kept paused
 * @return none
 */
void startup_screen_apply_wallpaper_prefs(void)
{
    if (!ui_bg_image || !sg_shake_timer) {
        return;
    }

    memset(&sg_wall_list, 0, sizeof(sg_wall_list));
    __apply_home_builtin_bg();
    lv_timer_pause(sg_shake_timer);
}

static void startup_screen_init(void)
{
    ui_startup_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(ui_startup_screen);
    lv_obj_set_size(ui_startup_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(ui_startup_screen, LV_OPA_TRANSP, 0);
    /* Root must not participate in scroll chaining; otherwise indev may set scroll_obj and drop gestures. */
    lv_obj_remove_flag(ui_startup_screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_bg_image = lv_image_create(ui_startup_screen);
    lv_obj_set_size(ui_bg_image, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ui_bg_image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(ui_bg_image, LV_OBJ_FLAG_SCROLLABLE);

    memset(&sg_wall_dsc, 0, sizeof(sg_wall_dsc));
    memset(&sg_wall_list, 0, sizeof(sg_wall_list));

    ui_wifi_label = lv_label_create(ui_startup_screen);
    lv_obj_set_style_text_font(ui_wifi_label, lv_font_default(), 0);
    lv_label_set_text(ui_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(ui_wifi_label, lv_color_hex(0x00FF66), 0);
    lv_obj_align(ui_wifi_label, LV_ALIGN_TOP_MID, 0, 6);

    ui_clock_row = lv_obj_create(ui_startup_screen);
    lv_obj_remove_style_all(ui_clock_row);
    lv_obj_set_style_bg_opa(ui_clock_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_clock_row, 0, 0);
    lv_obj_set_style_pad_all(ui_clock_row, 0, 0);
    lv_obj_set_style_pad_column(ui_clock_row, 0, 0);
    lv_obj_set_flex_flow(ui_clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(ui_clock_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(ui_clock_row, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(ui_clock_row, LV_OBJ_FLAG_SCROLLABLE);

    ui_clock_hh = lv_label_create(ui_clock_row);
    ui_clock_colon = lv_label_create(ui_clock_row);
    ui_clock_mm = lv_label_create(ui_clock_row);
    __style_clock_hh_mm(ui_clock_hh);
    __style_clock_colon(ui_clock_colon);
    __style_clock_hh_mm(ui_clock_mm);
    lv_label_set_text(ui_clock_colon, ":");
    lv_label_set_long_mode(ui_clock_hh, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(ui_clock_colon, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(ui_clock_mm, LV_LABEL_LONG_CLIP);

    sg_clock_colon_on = 1;
    sg_clock_hhmm_cache_valid = 0;
    lv_obj_set_style_text_opa(ui_clock_colon, LV_OPA_COVER, 0);
    __update_clock_label();
    sg_clock_timer = lv_timer_create(__clock_timer_cb, 1000, NULL);
    sg_shake_timer = lv_timer_create(__shake_timer_cb, 45, NULL);

    __apply_home_builtin_bg();
    lv_timer_pause(sg_shake_timer);

    {
        lv_obj_t *bell;
        lv_obj_t *badge_dot;
        const lv_font_t *bf;

        ui_notice_hit = lv_obj_create(ui_startup_screen);
        lv_obj_remove_style_all(ui_notice_hit);
        lv_obj_set_size(ui_notice_hit, DUCKY_NOTICE_HIT_SIDE, DUCKY_NOTICE_HIT_SIDE);
        lv_obj_align(ui_notice_hit, LV_ALIGN_BOTTOM_MID, 0, -16);
        lv_obj_set_style_bg_opa(ui_notice_hit, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ui_notice_hit, 0, 0);
        lv_obj_add_flag(ui_notice_hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ui_notice_hit, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_remove_flag(ui_notice_hit, LV_OBJ_FLAG_SCROLLABLE);

        bell = lv_label_create(ui_notice_hit);
        lv_obj_set_style_text_font(bell, __notice_bell_font(), 0);
        lv_label_set_text(bell, FONT_AWESOME_BELL);
        lv_obj_set_style_text_color(bell, lv_color_hex(DUCKY_NOTICE_BELL_BLUE), 0);
        lv_obj_center(bell);
        lv_obj_remove_flag(bell, LV_OBJ_FLAG_CLICKABLE);

        badge_dot = lv_obj_create(ui_notice_hit);
        lv_obj_remove_style_all(badge_dot);
        lv_obj_set_style_bg_color(badge_dot, lv_color_hex(DUCKY_NOTICE_BADGE_RED), 0);
        lv_obj_set_style_bg_opa(badge_dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_hor(badge_dot, 7, 0);
        lv_obj_set_style_pad_ver(badge_dot, 4, 0);
        lv_obj_set_style_border_width(badge_dot, 0, 0);
        lv_obj_set_size(badge_dot, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_remove_flag(badge_dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(badge_dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(badge_dot, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(badge_dot, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        bf = ai_ui_get_text_font();
        ui_notice_badge_lbl = lv_label_create(badge_dot);
        lv_obj_set_style_text_font(ui_notice_badge_lbl, bf ? bf : lv_font_default(), 0);
        lv_obj_set_style_text_color(ui_notice_badge_lbl, lv_color_white(), 0);
        lv_label_set_text(ui_notice_badge_lbl, "1");
        lv_label_set_long_mode(ui_notice_badge_lbl, LV_LABEL_LONG_CLIP);
        lv_obj_remove_flag(ui_notice_badge_lbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(ui_notice_badge_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_center(ui_notice_badge_lbl);

        lv_obj_align_to(badge_dot, bell, LV_ALIGN_OUT_TOP_RIGHT, 2, -6);

        lv_obj_add_event_cb(ui_notice_hit, __on_notice_bell_clicked, LV_EVENT_CLICKED, NULL);
        __notice_badge_apply();
    }

    lv_obj_move_foreground(ui_wifi_label);
    lv_obj_move_foreground(ui_clock_row);
    if (ui_notice_hit) {
        lv_obj_move_foreground(ui_notice_hit);
    }

    nav_gesture_attach(ui_startup_screen, 1, LV_DIR_NONE);
}

static void startup_screen_deinit(void)
{
    if (sg_clock_timer) {
        lv_timer_del(sg_clock_timer);
        sg_clock_timer = NULL;
    }
    if (sg_shake_timer) {
        lv_timer_del(sg_shake_timer);
        sg_shake_timer = NULL;
    }

    wallpaper_free_decoded(&sg_wall_dsc);
    memset(&sg_wall_list, 0, sizeof(sg_wall_list));

    ui_bg_image = NULL;
    ui_wifi_label = NULL;
    ui_clock_row = NULL;
    ui_clock_hh = NULL;
    ui_clock_colon = NULL;
    ui_clock_mm = NULL;
    ui_notice_hit       = NULL;
    ui_notice_badge_lbl = NULL;
    ui_startup_screen = NULL;
    sg_clock_colon_on = 1;
    sg_clock_hhmm_cache_valid = 0;
}
