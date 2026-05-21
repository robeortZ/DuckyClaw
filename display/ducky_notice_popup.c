/**
 * @file ducky_notice_popup.c
 * @brief Notice popup for DP101 (pipe or JSON)
 * @version 1.0
 * @date 2026-03-27
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ducky_notice_popup.h"

#include "ducky_notice_history.h"
#include "ducky_notice_unread.h"
#include "startup_screen.h"
#include "cJSON.h"
#include "lvgl.h"
#include "tal_api.h"
#include "tal_memory.h"

#include "ai_ui_icon_font.h"
#include "ducky_ui_strings.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define DUCKY_NOTICE_POPUP_AUTO_CLOSE_MS 8000
#define DUCKY_NOTICE_TEXT_MAX            384
#define DUCKY_NOTICE_PIPE_PARSE_TMP_SZ   640

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static lv_obj_t *s_notice_layer;

/* ---------------------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------------------- */
static void ducky_notice_popup_close(void);

static void __badge_refresh_async(void *ud)
{
    (void)ud;
    startup_screen_notice_badge_refresh();
}

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Case-insensitive compare first segment to "notice"
 * @param[in] s segment text
 * @param[in] len segment length
 * @return 1 if notice
 */
static int __seg_is_notice(const char *s, size_t len)
{
    const char want[] = "notice";
    size_t       i;

    if (len != 6) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        if (c != want[i]) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Extract notice content from pipe format (type|id|time|content|...)
 * @param[in] raw full DP string
 * @param[out] out notice body
 * @param[in] out_sz buffer size
 * @return OPRT_OK if notice and content copied
 */
static OPERATE_RET __parse_pipe_notice(const char *raw, char *out, size_t out_sz)
{
    char   tmp[DUCKY_NOTICE_PIPE_PARSE_TMP_SZ];
    char * parts[6];
    int    i;
    int    p;
    size_t n;

    if (!raw || !out || out_sz == 0) {
        return OPRT_INVALID_PARM;
    }

    n = strlen(raw);
    if (n == 0 || n >= sizeof(tmp)) {
        return OPRT_INVALID_PARM;
    }
    memcpy(tmp, raw, n + 1);

    p = 0;
    parts[p++] = tmp;
    for (i = 0; tmp[i] && p < 6; i++) {
        if (tmp[i] == '|') {
            tmp[i] = '\0';
            if (p < 6) {
                parts[p++] = tmp + i + 1;
            }
        }
    }

    if (p < 4 || !parts[0] || !parts[3]) {
        return OPRT_NOT_FOUND;
    }

    n = strlen(parts[0]);
    if (!__seg_is_notice(parts[0], n)) {
        return OPRT_NOT_FOUND;
    }

    (void)snprintf(out, out_sz, "%s", parts[3]);
    return OPRT_OK;
}

/**
 * @brief DP101 may pack several pipe-records in one string, one line each; find notice line
 * @param[in] raw multi-line or single-line pipe text
 * @param[out] out notice body
 * @param[in] out_sz buffer size
 * @return OPRT_OK if a notice line was found
 */
static OPERATE_RET __parse_pipe_lines_for_notice(const char *raw, char *out, size_t out_sz)
{
    const char *line_start = raw;

    if (!raw || !out || out_sz == 0) {
        return OPRT_INVALID_PARM;
    }

    while (line_start && *line_start) {
        const char *nl = strchr(line_start, '\n');
        size_t      linelen;
        char        line_buf[256];
        char *      p;

        if (nl) {
            linelen = (size_t)(nl - line_start);
        } else {
            linelen = strlen(line_start);
        }
        if (linelen >= sizeof(line_buf)) {
            linelen = sizeof(line_buf) - 1;
        }
        memcpy(line_buf, line_start, linelen);
        line_buf[linelen] = '\0';
        while (linelen > 0 && (line_buf[linelen - 1] == '\r' || line_buf[linelen - 1] == ' ')) {
            line_buf[--linelen] = '\0';
        }
        p = line_buf;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p != '\0' && __parse_pipe_notice(p, out, out_sz) == OPRT_OK) {
            return OPRT_OK;
        }
        if (!nl) {
            break;
        }
        line_start = nl + 1;
    }
    return OPRT_NOT_FOUND;
}

/**
 * @brief Extract first notice content from JSON {"items":[{"type":"notice","content":"..."}]}
 * @param[in] raw JSON string
 * @param[out] out notice body
 * @param[in] out_sz buffer size
 * @return OPRT_OK if found
 */
static OPERATE_RET __parse_json_notice(const char *raw, char *out, size_t out_sz)
{
    cJSON * root  = NULL;
    cJSON * items = NULL;
    cJSON * it    = NULL;
    int     ok    = 0;

    if (!raw || !out || out_sz == 0) {
        return OPRT_INVALID_PARM;
    }

    root = cJSON_Parse(raw);
    if (!root) {
        return OPRT_NOT_FOUND;
    }

    items = cJSON_GetObjectItem(root, "items");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return OPRT_NOT_FOUND;
    }

    cJSON_ArrayForEach(it, items)
    {
        cJSON *type    = cJSON_GetObjectItem(it, "type");
        cJSON *content = cJSON_GetObjectItem(it, "content");
        if (cJSON_IsString(type) && cJSON_IsString(content) && type->valuestring && content->valuestring) {
            if (__seg_is_notice(type->valuestring, strlen(type->valuestring))) {
                (void)snprintf(out, out_sz, "%s", content->valuestring);
                ok = 1;
                break;
            }
        }
    }

    cJSON_Delete(root);
    return ok ? OPRT_OK : OPRT_NOT_FOUND;
}

/**
 * @brief Fill out buffer when payload is a notice
 * @param[in] raw DP string
 * @param[out] out text to show
 * @param[in] out_sz cap
 * @return OPRT_OK if notice
 */
static OPERATE_RET __parse_notice_body(const char *raw, char *out, size_t out_sz)
{
    OPERATE_RET ret;
    const char *p;

    if (!raw || !out || out_sz == 0) {
        return OPRT_INVALID_PARM;
    }

    p = raw;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }

    if (*p == '{') {
        ret = __parse_json_notice(p, out, out_sz);
        if (ret == OPRT_OK) {
            return OPRT_OK;
        }
    }

    /* Multi-line body must use whole-string parse: content may contain '\\n' between pipes.
     * Line-based parse is only when a second record starts with "notice|" on a new line. */
    if (strstr(p, "\nnotice|") != NULL) {
        ret = __parse_pipe_lines_for_notice(p, out, out_sz);
        if (ret == OPRT_OK) {
            return OPRT_OK;
        }
    }
    ret = __parse_pipe_notice(p, out, out_sz);
    if (ret == OPRT_OK) {
        return OPRT_OK;
    }
    return __parse_pipe_lines_for_notice(p, out, out_sz);
}

/**
 * @brief Remove popup layer
 * @return none
 */
static void ducky_notice_popup_close(void)
{
    if (s_notice_layer) {
        lv_obj_del(s_notice_layer);
        s_notice_layer = NULL;
    }
}

/**
 * @brief Auto-close timer
 * @param[in] t timer
 * @return none
 */
static void __notice_auto_close_cb(lv_timer_t *t)
{
    if (t) {
        lv_timer_delete(t);
    }
    ducky_notice_popup_close();
}

/**
 * @brief Tap backdrop to dismiss
 * @param[in] e LVGL event
 * @return none
 */
static void __notice_backdrop_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ducky_notice_popup_close();
}

/**
 * @brief Show modal (call with LVGL lock held)
 * @param[in] message body text
 * @return none
 */
static void ducky_notice_popup_show_impl(const char *message)
{
    lv_obj_t *shade;
    lv_obj_t *box;
    lv_obj_t *title;
    lv_obj_t *body;
    lv_font_t *font;

    if (!message || !message[0]) {
        return;
    }

    ducky_notice_popup_close();

    font = ai_ui_get_text_font();

    shade = lv_obj_create(lv_layer_top());
    s_notice_layer = shade;
    lv_obj_set_size(shade, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(shade, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(shade, lv_color_black(), 0);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shade, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(shade, __notice_backdrop_clicked, LV_EVENT_CLICKED, NULL);

    box = lv_obj_create(shade);
    lv_obj_set_width(box, LV_PCT(86));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x2c2c2c), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_pad_all(box, 14, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(box, 10, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);

    title = lv_label_create(box);
    if (font) {
        lv_obj_set_style_text_font(title, font, 0);
    }
    lv_label_set_text(title, ducky_ui_str(DUCKY_UI_STR_NOTICE_POPUP));
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);

    body = lv_label_create(box);
    if (font) {
        lv_obj_set_style_text_font(body, font, 0);
    }
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_text(body, message);
    lv_obj_set_style_text_color(body, lv_color_hex(0xEEEEEE), 0);

    (void)lv_timer_create(__notice_auto_close_cb, DUCKY_NOTICE_POPUP_AUTO_CLOSE_MS, NULL);
}

typedef struct {
    char text[DUCKY_NOTICE_TEXT_MAX];
} ducky_notice_async_t;

/**
 * @brief Run on LVGL thread via lv_async_call
 * @param[in] user_data ducky_notice_async_t *
 * @return none
 */
static void __notice_async_cb(void *user_data)
{
    ducky_notice_async_t *d = (ducky_notice_async_t *)user_data;

    if (!d) {
        return;
    }
    /* lv_async_call runs inside lv_task_handler(); lv_vendor thread already holds g_disp_mutex. */
    ducky_notice_popup_show_impl(d->text);
    tal_free(d);
}

/**
 * @brief Parse DP101 string; if it describes a notice, queue a modal on the display.
 * @param[in] raw_dp_str string from dp->value.dp_str (pipe or JSON with items[])
 * @return none
 */
void ducky_notice_handle_dp_string(const char *raw_dp_str)
{
    ducky_notice_async_t *d;
    char                    body[DUCKY_NOTICE_TEXT_MAX];
    lv_result_t             ar;

    if (!raw_dp_str || !raw_dp_str[0]) {
        return;
    }

    memset(body, 0, sizeof(body));
    if (__parse_notice_body(raw_dp_str, body, sizeof(body)) != OPRT_OK) {
        return;
    }

    ducky_notice_history_push(body);
    ducky_notice_unread_add_one();
    (void)lv_async_call(__badge_refresh_async, NULL);

    d = (ducky_notice_async_t *)tal_malloc(sizeof(*d));
    if (!d) {
        return;
    }
    memset(d, 0, sizeof(*d));
    (void)snprintf(d->text, sizeof(d->text), "%s", body);

    ar = lv_async_call(__notice_async_cb, d);
    if (ar != LV_RESULT_OK) {
        tal_free(d);
    }
}
