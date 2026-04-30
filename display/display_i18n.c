/**
 * @file display_i18n.c
 * @brief 多语言支持实现
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_i18n.h"

static display_lang_t sg_lang = LANG_ZH;

static const char *sg_strings_zh[STR_MAX] = {
    [STR_HOME]         = "主页",
    [STR_MENU]         = "菜单",
    [STR_CRON]         = "定时任务",
    [STR_FOLDER]       = "文件夹",
    [STR_SETTING]      = "设置",
    [STR_READER]       = "阅读器",
    [STR_NOTIFICATION] = "通知",
    [STR_HISTORY]      = "历史通知",
    [STR_BACKLIGHT]    = "背光",
    [STR_SLEEP_TIME]   = "休眠时间",
    [STR_LANGUAGE]     = "语言",
    [STR_RESET_DEVICE] = "重置设备",
    [STR_LANG_VALUE]   = "中文",
    [STR_NO_TASK]      = "暂无定时任务",
    [STR_NO_NOTIF]     = "暂无通知",
    [STR_OPEN_FAIL]    = "无法打开文件",
    [STR_SUN]          = "周日",
    [STR_MON]          = "周一",
    [STR_TUE]          = "周二",
    [STR_WED]          = "周三",
    [STR_THU]          = "周四",
    [STR_FRI]          = "周五",
    [STR_SAT]          = "周六",
};

static const char *sg_strings_en[STR_MAX] = {
    [STR_HOME]         = "Home",
    [STR_MENU]         = "Menu",
    [STR_CRON]         = "Timers",
    [STR_FOLDER]       = "Files",
    [STR_SETTING]      = "Settings",
    [STR_READER]       = "Reader",
    [STR_NOTIFICATION] = "Notification",
    [STR_HISTORY]      = "History",
    [STR_BACKLIGHT]    = "Backlight",
    [STR_SLEEP_TIME]   = "Sleep Time",
    [STR_LANGUAGE]     = "Language",
    [STR_RESET_DEVICE] = "Reset Device",
    [STR_LANG_VALUE]   = "English",
    [STR_NO_TASK]      = "No timers",
    [STR_NO_NOTIF]     = "No notifications",
    [STR_OPEN_FAIL]    = "Cannot open file",
    [STR_SUN]          = "Sun",
    [STR_MON]          = "Mon",
    [STR_TUE]          = "Tue",
    [STR_WED]          = "Wed",
    [STR_THU]          = "Thu",
    [STR_FRI]          = "Fri",
    [STR_SAT]          = "Sat",
};

display_lang_t i18n_get_lang(void)
{
    return sg_lang;
}

void i18n_set_lang(display_lang_t lang)
{
    sg_lang = lang;
}

void i18n_toggle_lang(void)
{
    sg_lang = (sg_lang == LANG_ZH) ? LANG_EN : LANG_ZH;
}

const char *i18n_str(str_id_t id)
{
    if (id >= STR_MAX) return "";
    const char *s = (sg_lang == LANG_ZH) ? sg_strings_zh[id] : sg_strings_en[id];
    return s ? s : "";
}
