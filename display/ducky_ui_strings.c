/**
 * @file ducky_ui_strings.c
 * @brief KV-backed UI language and string tables
 * @version 1.0
 * @date 2026-04-01
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ducky_ui_strings.h"

#include "tal_api.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#define CLAW_KV_UI_LANG_EN "claw_ui_lang_en"

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static uint8_t s_lang_en = 1;

/* Order must match ducky_ui_string_id_e */
static const char *const s_en[DUCKY_UI_STR_COUNT] = {
    "Menu",
    "To-do",
    "Timers",
    "Music",
    "Settings",
    "Settings",
    "Brightness",
    "Volume",
    "Shake to change wallpaper",
    "Language",
    "Reset pairing",
    "Swipe up or right to go back",
    "Notice",
    "Messages",
    "No messages yet",
    "Swipe up or right to go back · Pull down on home",
    "Tap to return",
    "No to-do items",
    "To-do",
    "No scheduled tasks",
    "Scheduled tasks",
    "Nothing playing",
    "Ask by voice to play music",
    "AI audio not enabled",
    "Previous track",
    "Next track",
    "Play music",
};

static const char *const s_zh[DUCKY_UI_STR_COUNT] = {
    "菜单",
    "待办",
    "定时",
    "音乐",
    "设置",
    "设置",
    "亮度",
    "音量",
    "摇一摇换壁纸",
    "语言",
    "重置配网",
    "上滑或右滑返回上一级",
    "通知",
    "消息推送",
    "暂无推送消息",
    "上滑或右滑返回 · 主页下拉打开",
    "点击屏幕返回",
    "暂无待办事项",
    "待办列表",
    "暂无定时任务",
    "定时任务",
    "暂无播放",
    "通过语音点播音乐",
    "未启用 AI 音频模块",
    "上一首",
    "下一首",
    "播放音乐",
};

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Load UI language from KV (call once before UI uses strings)
 * @return none
 */
void ducky_ui_lang_init(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;
    uint8_t   default_en = 1;

    s_lang_en = 1;
    if (tal_kv_get(CLAW_KV_UI_LANG_EN, &buf, &len) != OPRT_OK || buf == NULL || len < 1) {
        if (buf) {
            tal_kv_free(buf);
        }
        (void)tal_kv_set(CLAW_KV_UI_LANG_EN, &default_en, 1);
        return;
    }
    s_lang_en = buf[0] ? 1 : 0;
    tal_kv_free(buf);
}

/**
 * @brief Whether UI uses English (1) or Chinese (0)
 * @return 1 English, 0 Chinese
 */
uint8_t ducky_ui_lang_en_get(void)
{
    return s_lang_en;
}

/**
 * @brief Set UI language and persist
 * @param[in] en 1 English, 0 Chinese
 * @return OPRT_OK on success
 */
OPERATE_RET ducky_ui_lang_en_set(uint8_t en)
{
    uint8_t v = en ? 1 : 0;

    s_lang_en = v;
    return tal_kv_set(CLAW_KV_UI_LANG_EN, &v, 1);
}

/**
 * @brief Localized string for current language
 * @param[in] id string id
 * @return literal C string, empty if id invalid
 */
const char *ducky_ui_str(ducky_ui_string_id_e id)
{
    if (id >= DUCKY_UI_STR_COUNT) {
        return "";
    }
    return s_lang_en ? s_en[id] : s_zh[id];
}
