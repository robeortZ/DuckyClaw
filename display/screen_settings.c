/**
 * @file screen_settings.c
 * @brief Settings: brightness / volume sliders and reset pairing (round safe area, compact width)
 */
#include "screen_settings.h"
#include "nav_gesture.h"
#include "ui_layout.h"
#include "ai_ui_icon_font.h"
#include "lv_port_disp.h"
#include "ai_chat_main.h"
#include "reset_netcfg.h"
#include "claw_wallpaper_prefs.h"
#include "startup_screen.h"
#include "tal_log.h"
#include "ducky_ui_strings.h"
#include "tuya_iot.h"
#include "ai_audio_player.h"
#include "tal_system.h"

#include "lvgl.h"

static lv_obj_t *ui_scr;
static lv_obj_t *sl_brightness;
static lv_obj_t *sl_volume;

static lv_obj_t *s_lbl_title;
static lv_obj_t *s_lbl_brightness;
static lv_obj_t *s_lbl_volume;
static lv_obj_t *s_lbl_shake;
static lv_obj_t *s_lbl_lang;
static lv_obj_t *s_lbl_reset;
static lv_obj_t *s_lbl_hint;
static lv_obj_t *s_sw_lang;

/** Limit backlight slider callback rate during drag. */
#define DUCKY_SL_BRIGHTNESS_MIN_INTERVAL_MS 40U

static uint8_t s_brightness = 80;
static uint32_t s_brightness_last_apply_ms;
static bool     s_brightness_cb_guard;

static void screen_settings_init(void);
static void screen_settings_deinit(void);

static void __settings_refresh_labels(void);

/**
 * @brief Clamp volume slider value to the same range used at init (0–95).
 * @param[in] v Raw slider value
 * @return Clamped value
 */
static int32_t __volume_clamp(int32_t v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 95) {
        return 95;
    }
    return v;
}

/**
 * @brief Apply backlight from slider, clamp to hardware range, sync knob if UI exceeds clamp.
 * @param[in] slider Brightness slider object
 * @return none
 */
static void __brightness_apply(lv_obj_t *slider)
{
    int32_t v = lv_slider_get_value(slider);

    if (v < 5) {
        v = 5;
    }
    if (v > 97) {
        v = 97;
    }
    disp_set_backlight(lv_display_get_default(), (uint8_t)v);
    s_brightness = (uint8_t)v;
    if (lv_slider_get_value(slider) != v) {
        s_brightness_cb_guard = true;
        lv_slider_set_value(slider, v, LV_ANIM_OFF);
        s_brightness_cb_guard = false;
    }
}

Screen_t screen_settings = {
    .init = screen_settings_init,
    .deinit = screen_settings_deinit,
    .screen_obj = &ui_scr,
    .name = "Settings",
};

/**
 * @brief Brightness slider: throttle VALUE_CHANGED; always apply on RELEASED for final value.
 * @param[in] e LVGL event
 * @return none
 */
static void __on_brightness(lv_event_t *e)
{
    lv_obj_t         *slider = lv_event_get_target(e);
    lv_event_code_t   code = lv_event_get_code(e);

    if (s_brightness_cb_guard) {
        return;
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t now = tal_system_get_millisecond();

        if ((now - s_brightness_last_apply_ms) < DUCKY_SL_BRIGHTNESS_MIN_INTERVAL_MS) {
            return;
        }
        s_brightness_last_apply_ms = now;
    }
    __brightness_apply(slider);
}

/**
 * @brief Volume slider drag: update player only (no KV each frame — avoids flash/heap pressure).
 * @param[in] e LVGL event
 * @return none
 */
static void __on_volume_drag(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   v = __volume_clamp(lv_slider_get_value(slider));

    (void)ai_audio_player_set_vol((int)v);
}

/**
 * @brief Volume slider release: persist volume once via ai_chat_set_volume (tal_kv_set).
 * @param[in] e LVGL event
 * @return none
 */
static void __on_volume_release(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t   v = __volume_clamp(lv_slider_get_value(slider));

    (void)ai_chat_set_volume((int)v);
}

static void __on_reset_btn(lv_event_t *e)
{
    (void)e;
    PR_WARN("User triggered reset_netconfig from settings UI");
    tuya_iot_reset(tuya_iot_client_get());
}

static void __on_shake_wp_switch(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    uint8_t   on;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    on = (lv_obj_get_state(sw) & LV_STATE_CHECKED) ? (uint8_t)1 : (uint8_t)0;
    (void)claw_wallpaper_prefs_shake_enabled_set(on);
    startup_screen_apply_wallpaper_prefs();
}

static void __on_lang_switch(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    uint8_t   en;

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    en = (lv_obj_get_state(sw) & LV_STATE_CHECKED) ? (uint8_t)1 : (uint8_t)0;
    (void)ducky_ui_lang_en_set(en);
    __settings_refresh_labels();
}

static void __settings_refresh_labels(void)
{
    if (s_lbl_title) {
        lv_label_set_text(s_lbl_title, ducky_ui_str(DUCKY_UI_STR_SETTINGS_TITLE));
    }
    if (s_lbl_brightness) {
        lv_label_set_text(s_lbl_brightness, ducky_ui_str(DUCKY_UI_STR_BRIGHTNESS));
    }
    if (s_lbl_volume) {
        lv_label_set_text(s_lbl_volume, ducky_ui_str(DUCKY_UI_STR_VOLUME));
    }
    if (s_lbl_shake) {
        lv_label_set_text(s_lbl_shake, ducky_ui_str(DUCKY_UI_STR_SHAKE_WALLPAPER));
    }
    if (s_lbl_lang) {
        lv_label_set_text(s_lbl_lang, ducky_ui_str(DUCKY_UI_STR_SETTINGS_LANG));
    }
    if (s_lbl_reset) {
        lv_label_set_text(s_lbl_reset, ducky_ui_str(DUCKY_UI_STR_RESET_NETCFG));
    }
    if (s_lbl_hint) {
        lv_label_set_text(s_lbl_hint, ducky_ui_str(DUCKY_UI_STR_HINT_BACK_LEVEL));
    }
}

/** Slider track / knob colors aligned with settings accent (cyan on dark) */
#define DUCKY_SET_SL_MAIN      0x1A3545
#define DUCKY_SET_ACCENT       0x00C8E8
#define DUCKY_SET_LABEL        0xE8E8E8

/**
 * @brief Minimal slider styling to limit LVGL style-node / draw RAM on small PSRAM heaps.
 * @param[in] sl Slider object
 * @return none
 */
static void __style_settings_slider(lv_obj_t *sl)
{
    lv_obj_set_height(sl, 22);
    lv_obj_set_style_bg_color(sl, lv_color_hex(DUCKY_SET_SL_MAIN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(DUCKY_SET_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(DUCKY_SET_ACCENT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
}

static void screen_settings_init(void)
{
    lv_font_t *txt_font = ai_ui_get_text_font();
    lv_obj_t * btn;
    lv_obj_t * bl;

    s_lbl_title = NULL;
    s_lbl_brightness = NULL;
    s_lbl_volume = NULL;
    s_lbl_shake = NULL;
    s_lbl_lang = NULL;
    s_lbl_reset = NULL;
    s_lbl_hint = NULL;
    s_sw_lang = NULL;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x202020), 0);
    lv_obj_remove_flag(ui_scr, LV_OBJ_FLAG_SCROLLABLE);
    /* One flex column on the screen: avoids an extra "outer" container (saves LVGL object + style RAM). */
    lv_obj_set_style_pad_hor(ui_scr, DUCKY_ROUND_PAD_X, 0);
    lv_obj_set_style_pad_ver(ui_scr, DUCKY_ROUND_PAD_Y, 0);
    lv_obj_set_flex_flow(ui_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui_scr, 8, 0);
    lv_obj_set_flex_align(ui_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (txt_font) {
        lv_obj_set_style_text_font(ui_scr, txt_font, 0);
    }

    /* Default UI language is English when KV has no claw_ui_lang_en yet (see ducky_ui_lang_init). */
    ducky_ui_lang_init();

    s_lbl_title = lv_label_create(ui_scr);
    lv_obj_set_width(s_lbl_title, lv_pct(100));
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_lbl_title, LV_TEXT_ALIGN_CENTER, 0);

    s_lbl_brightness = lv_label_create(ui_scr);
    lv_obj_set_width(s_lbl_brightness, lv_pct(76));
    lv_obj_set_style_text_color(s_lbl_brightness, lv_color_hex(DUCKY_SET_LABEL), 0);

    sl_brightness = lv_slider_create(ui_scr);
    lv_obj_remove_flag(sl_brightness, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(sl_brightness, lv_pct(76));
    __style_settings_slider(sl_brightness);
    lv_slider_set_range(sl_brightness, 5, 100);
    lv_slider_set_value(sl_brightness, s_brightness, LV_ANIM_OFF);
    s_brightness_last_apply_ms = tal_system_get_millisecond();
    lv_obj_add_event_cb(sl_brightness, __on_brightness, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_brightness, __on_brightness, LV_EVENT_RELEASED, NULL);
    /* Stop swipe-back (LV_EVENT_GESTURE on screen) from firing when dragging the horizontal slider. */
    lv_obj_remove_flag(sl_brightness, LV_OBJ_FLAG_GESTURE_BUBBLE);
    disp_set_backlight(lv_display_get_default(), s_brightness);

    s_lbl_volume = lv_label_create(ui_scr);
    lv_obj_set_width(s_lbl_volume, lv_pct(76));
    lv_obj_set_style_text_color(s_lbl_volume, lv_color_hex(DUCKY_SET_LABEL), 0);

    sl_volume = lv_slider_create(ui_scr);
    lv_obj_remove_flag(sl_volume, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(sl_volume, lv_pct(76));
    __style_settings_slider(sl_volume);
    lv_slider_set_range(sl_volume, 0, 100);
    {
        int vol = ai_chat_get_volume();

        if (vol < 0) {
            vol = 0;
        }
        if (vol > 95) {
            vol = 95;
        }
        lv_slider_set_value(sl_volume, vol, LV_ANIM_OFF);
    }
    lv_obj_add_event_cb(sl_volume, __on_volume_drag, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sl_volume, __on_volume_release, LV_EVENT_RELEASED, NULL);
    lv_obj_remove_flag(sl_volume, LV_OBJ_FLAG_GESTURE_BUBBLE);

    {
        lv_obj_t *row_sw = lv_obj_create(ui_scr);
        lv_obj_t *sw_shake;

        lv_obj_set_width(row_sw, lv_pct(76));
        lv_obj_set_height(row_sw, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row_sw, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_sw, 0, 0);
        lv_obj_set_style_pad_all(row_sw, 2, 0);
        lv_obj_remove_flag(row_sw, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row_sw, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_sw, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_lbl_shake = lv_label_create(row_sw);
        lv_obj_set_style_text_color(s_lbl_shake, lv_color_hex(DUCKY_SET_LABEL), 0);

        sw_shake = lv_switch_create(row_sw);
        lv_obj_set_style_bg_color(sw_shake, lv_color_hex(DUCKY_SET_SL_MAIN), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw_shake, lv_color_hex(DUCKY_SET_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (claw_wallpaper_prefs_shake_enabled_get()) {
            lv_obj_add_state(sw_shake, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(sw_shake, __on_shake_wp_switch, LV_EVENT_VALUE_CHANGED, NULL);
    }

    {
        lv_obj_t *row_lang = lv_obj_create(ui_scr);

        lv_obj_set_width(row_lang, lv_pct(76));
        lv_obj_set_height(row_lang, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row_lang, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_lang, 0, 0);
        lv_obj_set_style_pad_all(row_lang, 2, 0);
        lv_obj_remove_flag(row_lang, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row_lang, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_lang, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_lbl_lang = lv_label_create(row_lang);
        lv_obj_set_flex_grow(s_lbl_lang, 1);
        lv_obj_set_style_text_color(s_lbl_lang, lv_color_hex(DUCKY_SET_LABEL), 0);

        s_sw_lang = lv_switch_create(row_lang);
        lv_obj_set_style_bg_color(s_sw_lang, lv_color_hex(DUCKY_SET_SL_MAIN), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_sw_lang, lv_color_hex(DUCKY_SET_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (ducky_ui_lang_en_get()) {
            lv_obj_add_state(s_sw_lang, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(s_sw_lang, __on_lang_switch, LV_EVENT_VALUE_CHANGED, NULL);
    }

    btn = lv_button_create(ui_scr);
    lv_obj_set_width(btn, lv_pct(58));
    lv_obj_set_style_bg_color(btn, lv_color_hex(DUCKY_SET_ACCENT), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_set_style_pad_ver(btn, 10, 0);

    bl = lv_label_create(btn);
    s_lbl_reset = bl;
    lv_obj_set_style_text_color(bl, lv_color_hex(0x101820), 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, __on_reset_btn, LV_EVENT_CLICKED, NULL);

    s_lbl_hint = lv_label_create(ui_scr);
    lv_obj_set_style_text_color(s_lbl_hint, lv_color_hex(0x939393), 0);
    lv_obj_set_width(s_lbl_hint, lv_pct(88));
    lv_obj_set_style_text_align(s_lbl_hint, LV_TEXT_ALIGN_CENTER, 0);
    /* WRAP allocates extra label layout RAM; DOT truncates on one line. */
    lv_label_set_long_mode(s_lbl_hint, LV_LABEL_LONG_DOT);
    lv_obj_remove_flag(s_lbl_hint, LV_OBJ_FLAG_SCROLLABLE);

    __settings_refresh_labels();

    nav_gesture_attach(ui_scr, 0, LV_DIR_NONE);
}

static void screen_settings_deinit(void)
{
    sl_brightness = NULL;
    sl_volume = NULL;
    s_lbl_title = NULL;
    s_lbl_brightness = NULL;
    s_lbl_volume = NULL;
    s_lbl_shake = NULL;
    s_lbl_lang = NULL;
    s_lbl_reset = NULL;
    s_lbl_hint = NULL;
    s_sw_lang = NULL;
    ui_scr = NULL;
}
