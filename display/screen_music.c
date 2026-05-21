/**
 * @file screen_music.c
 * @brief Music playback UI: dark frosted-style layout, track meta from ai_audio_player cache
 */
#include "screen_music.h"
#include "nav_gesture.h"
#include "tuya_cloud_types.h"
#include "ui_layout.h"
#include "ai_ui_icon_font.h"
#include "screen_manager.h"
#include "ducky_ui_strings.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
#include "ai_agent.h"
#include "ai_audio_player.h"
#include "ai_chat_main.h"
#endif

extern const lv_image_dsc_t yinle;

static lv_obj_t *       ui_scr;
static lv_obj_t *      ui_title;
static lv_obj_t *      ui_artist;
static lv_obj_t *      ui_play_lbl;
static lv_obj_t *      ui_disc_img;
static lv_obj_t *      ui_bg_art;
static lv_timer_t *    s_music_timer;

#ifndef LV_SYMBOL_VOLUME_MAX
#define LV_SYMBOL_VOLUME_MAX "\xEF\x80\xA8"
#endif

static void screen_music_init(void);
static void screen_music_deinit(void);

/**
 * @brief Copy localized command into stack buffer and send (ai_agent expects mutable string)
 * @param[in] id command string id
 * @return none
 */
static void __music_send_ai_text(ducky_ui_string_id_e id)
{
    char              buf[64];
    const char *      s = ducky_ui_str(id);

    (void)snprintf(buf, sizeof(buf), "%s", s ? s : "");
    (void)ai_agent_send_text(buf);
}

Screen_t screen_music = {
    .init       = screen_music_init,
    .deinit     = screen_music_deinit,
    .screen_obj = &ui_scr,
    .name       = "Music",
};

#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)

/**
 * @brief Periodic refresh of title/artist and play/pause glyph
 * @param[in] t LVGL timer
 * @return none
 */
static void __music_ui_timer(lv_timer_t *t)
{
    AI_AUDIO_MUSIC_UI_META_T meta;
    const char *song = ducky_ui_str(DUCKY_UI_STR_MUSIC_IDLE_TITLE);
    const char *art  = ducky_ui_str(DUCKY_UI_STR_MUSIC_IDLE_ARTIST);
    AI_AUDIO_MUSIC_UI_STATE_E st;

    (void)t;
    if (!ui_scr || !lv_obj_is_valid(ui_scr) || !ui_title || !ui_artist || !ui_play_lbl) {
        return;
    }

    memset(&meta, 0, sizeof(meta));
    if (ai_audio_player_get_current_music_meta(&meta) == OPRT_OK) {
        if (meta.song_name && meta.song_name[0]) {
            song = meta.song_name;
        }
        if (meta.artist && meta.artist[0]) {
            art = meta.artist;
        }
        /* Cover URL: embedded target has no HTTP image decoder here; keep local art. */
        (void)meta.img_url;
    }

    if (strcmp(lv_label_get_text(ui_title), song) != 0) {
        lv_label_set_text(ui_title, song);
    }
    if (strcmp(lv_label_get_text(ui_artist), art) != 0) {
        lv_label_set_text(ui_artist, art);
    }

    st = ai_audio_player_music_ui_state();
    if (st == AI_AUDIO_MUSIC_UI_PLAYING) {
        if (strcmp(lv_label_get_text(ui_play_lbl), LV_SYMBOL_PAUSE) != 0) {
            lv_label_set_text(ui_play_lbl, LV_SYMBOL_PAUSE);
        }
    } else {
        if (strcmp(lv_label_get_text(ui_play_lbl), LV_SYMBOL_PLAY) != 0) {
            lv_label_set_text(ui_play_lbl, LV_SYMBOL_PLAY);
        }
    }
}

/**
 * @brief Create a small icon-only control button
 * @param[in] parent Parent object
 * @param[in] symbol Label text (e.g. LV_SYMBOL_PREV)
 * @return Button object
 */
static lv_obj_t *__icon_btn(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *btn;
    lv_obj_t *lbl;

    btn = lv_button_create(parent);
    lv_obj_set_size(btn, 44, 44);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return btn;
}

static void __on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    screen_back();
}

static void __on_prev(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    /* Same as voice: cloud skill returns prev / playlist control */
    __music_send_ai_text(DUCKY_UI_STR_MUSIC_CMD_PREV);
}

static void __on_next(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    __music_send_ai_text(DUCKY_UI_STR_MUSIC_CMD_NEXT);
}

static void __on_play(lv_event_t *e)
{
    static BOOL_T is_first = FALSE;
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (ai_audio_player_music_ui_state() == AI_AUDIO_MUSIC_UI_PLAYING) {
        (void)ai_audio_player_music_pause_set(true);
        
    } else {
        (void)ai_audio_player_music_pause_set(false);
        // lv_label_set_text(ui_play_lbl, LV_SYMBOL_PAUSE);
        if (is_first == FALSE) {
            __music_send_ai_text(DUCKY_UI_STR_MUSIC_CMD_PLAY);
            is_first = TRUE;
        }
    }
    __music_ui_timer(NULL);
}

static void __on_vol(lv_event_t *e)
{
    int v;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    v = ai_chat_get_volume() + 10;
    if (v > 100) {
        v = 10;
    }
    (void)ai_chat_set_volume(v);
}

#endif /* ENABLE_COMP_AI_AUDIO */

static void screen_music_init(void)
{
    lv_font_t *  f = ai_ui_get_text_font();
    lv_obj_t *   panel;
    lv_obj_t *   disc_ring;
    lv_obj_t *   controls;
    lv_obj_t *   veil;

    ui_scr = lv_obj_create(NULL);
    lv_obj_set_size(ui_scr, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(ui_scr, lv_color_hex(0x1a1a1a), 0);
    lv_obj_remove_flag(ui_scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_bg_art = lv_image_create(ui_scr);
    lv_image_set_src(ui_bg_art, &yinle);
    lv_obj_set_size(ui_bg_art, LV_HOR_RES + 40, LV_VER_RES + 40);
    lv_obj_center(ui_bg_art);
    lv_obj_set_style_image_opa(ui_bg_art, LV_OPA_30, 0);
    lv_obj_set_style_transform_scale(ui_bg_art, (int32_t)((256 * 115) / 100), 0);
    lv_obj_remove_flag(ui_bg_art, LV_OBJ_FLAG_SCROLLABLE);

    veil = lv_obj_create(ui_scr);
    lv_obj_set_size(veil, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(veil, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(veil, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(veil, LV_OPA_50, 0);
    lv_obj_set_style_border_width(veil, 0, 0);
    lv_obj_remove_flag(veil, LV_OBJ_FLAG_SCROLLABLE);

    panel = lv_obj_create(ui_scr);
    lv_obj_set_size(panel, DUCKY_CONTENT_W, LV_VER_RES - 2 * DUCKY_ROUND_PAD_Y);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    if (f) {
        lv_obj_set_style_text_font(panel, f, 0);
    }

    /* DOT: no in-label swipe; avoids stealing LV_EVENT_GESTURE from nav_gesture on ui_scr. */
    ui_title = lv_label_create(panel);
    lv_label_set_long_mode(ui_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_title, lv_pct(92));
    lv_label_set_text(ui_title, ducky_ui_str(DUCKY_UI_STR_MUSIC_IDLE_TITLE));
    lv_obj_set_style_text_color(ui_title, lv_color_white(), 0);
    lv_obj_set_style_text_align(ui_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ui_title, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_remove_flag(ui_title, LV_OBJ_FLAG_SCROLLABLE);

    ui_artist = lv_label_create(panel);
    lv_label_set_long_mode(ui_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_artist, lv_pct(92));
    lv_label_set_text(ui_artist, ducky_ui_str(DUCKY_UI_STR_MUSIC_IDLE_ARTIST));
    lv_obj_set_style_text_color(ui_artist, lv_color_hex(0xb3b3b3), 0);
    lv_obj_set_style_text_align(ui_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(ui_artist, ui_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    lv_obj_remove_flag(ui_artist, LV_OBJ_FLAG_SCROLLABLE);

    disc_ring = lv_obj_create(panel);
    lv_obj_set_size(disc_ring, 210, 210);
    lv_obj_align(disc_ring, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_radius(disc_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc_ring, lv_color_hex(0x0d0d0d), 0);
    lv_obj_set_style_bg_opa(disc_ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(disc_ring, lv_color_hex(0x2e2e2e), 0);
    lv_obj_set_style_border_width(disc_ring, 10, 0);
    lv_obj_set_style_pad_all(disc_ring, 0, 0);
    lv_obj_set_scroll_dir(disc_ring, LV_DIR_NONE);
    lv_obj_remove_flag(disc_ring, LV_OBJ_FLAG_SCROLLABLE);

    ui_disc_img = lv_image_create(disc_ring);
    lv_image_set_src(ui_disc_img, &yinle);
    lv_obj_set_size(ui_disc_img, 178, 178);
    lv_obj_center(ui_disc_img);
    lv_obj_set_style_radius(ui_disc_img, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(ui_disc_img, true, 0);

    controls = lv_obj_create(panel);
    lv_obj_set_width(controls, lv_pct(100));
    lv_obj_set_height(controls, LV_SIZE_CONTENT);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_remove_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_text_font(controls, lv_font_default(), 0);
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    {
        lv_obj_t *b_back;
        lv_obj_t *b_prev;
        lv_obj_t *b_play;
        lv_obj_t *b_next;
        lv_obj_t *b_vol;

        b_back = __icon_btn(controls, LV_SYMBOL_LEFT);
        lv_obj_add_event_cb(b_back, __on_back, LV_EVENT_CLICKED, NULL);

        b_prev = __icon_btn(controls, LV_SYMBOL_PREV);
        lv_obj_add_event_cb(b_prev, __on_prev, LV_EVENT_CLICKED, NULL);

        b_play = lv_button_create(controls);
        lv_obj_set_size(b_play, 52, 52);
        lv_obj_set_style_radius(b_play, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(b_play, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(b_play, lv_color_white(), 0);
        lv_obj_set_style_border_width(b_play, 2, 0);
        lv_obj_set_style_shadow_width(b_play, 0, 0);
        ui_play_lbl = lv_label_create(b_play);
        lv_label_set_text(ui_play_lbl, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_color(ui_play_lbl, lv_color_white(), 0);
        lv_obj_center(ui_play_lbl);
        lv_obj_add_event_cb(b_play, __on_play, LV_EVENT_CLICKED, NULL);

        b_next = __icon_btn(controls, LV_SYMBOL_NEXT);
        lv_obj_add_event_cb(b_next, __on_next, LV_EVENT_CLICKED, NULL);

        b_vol = __icon_btn(controls, LV_SYMBOL_VOLUME_MAX);
        lv_obj_add_event_cb(b_vol, __on_vol, LV_EVENT_CLICKED, NULL);
    }

    s_music_timer = lv_timer_create(__music_ui_timer, 450, NULL);
    __music_ui_timer(NULL);
#else
    {
        lv_obj_t *hint = lv_label_create(controls);
        lv_label_set_text(hint, ducky_ui_str(DUCKY_UI_STR_MUSIC_AI_DISABLED));
        lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    }
#endif

    nav_gesture_attach(ui_scr, 0, LV_DIR_NONE);
}

static void screen_music_deinit(void)
{
#if defined(ENABLE_COMP_AI_AUDIO) && (ENABLE_COMP_AI_AUDIO == 1)
    if (s_music_timer) {
        lv_timer_delete(s_music_timer);
        s_music_timer = NULL;
    }
#endif
    ui_title    = NULL;
    ui_artist   = NULL;
    ui_play_lbl = NULL;
    ui_disc_img = NULL;
    ui_bg_art   = NULL;
    ui_scr      = NULL;
}
