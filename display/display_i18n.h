/**
 * @file display_i18n.h
 * @brief 多语言支持（中文/英文）
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_I18N_H__
#define __DISPLAY_I18N_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_ZH = 0,
    LANG_EN = 1,
} display_lang_t;

/* 字符串 ID */
typedef enum {
    STR_HOME,           /* 主页 / Home */
    STR_MENU,           /* 菜单 / Menu */
    STR_CRON,           /* 定时任务 / Timers */
    STR_FOLDER,         /* 文件夹 / Files */
    STR_SETTING,        /* 设置 / Settings */
    STR_READER,         /* 阅读器 / Reader */
    STR_NOTIFICATION,   /* 通知 / Notification */
    STR_HISTORY,        /* 历史通知 / History */
    STR_BACKLIGHT,      /* 背光 / Backlight */
    STR_SLEEP_TIME,     /* 休眠时间 / Sleep Time */
    STR_LANGUAGE,       /* 语言 / Language */
    STR_RESET_DEVICE,   /* 重置设备 / Reset Device */
    STR_LANG_VALUE,     /* 中文 / English */
    STR_NO_TASK,        /* 暂无定时任务 / No timers */
    STR_NO_NOTIF,       /* 暂无通知 / No notifications */
    STR_OPEN_FAIL,      /* 无法打开文件 / Cannot open file */
    STR_SUN,            /* 星期日 / Sun */
    STR_MON,            /* 星期一 / Mon */
    STR_TUE,            /* 星期二 / Tue */
    STR_WED,            /* 星期三 / Wed */
    STR_THU,            /* 星期四 / Thu */
    STR_FRI,            /* 星期五 / Fri */
    STR_SAT,            /* 星期六 / Sat */
    STR_MAX,
} str_id_t;

/**
 * @brief 获取当前语言
 */
display_lang_t i18n_get_lang(void);

/**
 * @brief 设置语言
 */
void i18n_set_lang(display_lang_t lang);

/**
 * @brief 切换语言（中<->英）
 */
void i18n_toggle_lang(void);

/**
 * @brief 根据 ID 获取当前语言的字符串
 */
const char *i18n_str(str_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_I18N_H__ */
