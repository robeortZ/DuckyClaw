/**
 * @file display_input.c
 * @brief LVGL 按键输入设备
 *
 * 将 6 个物理按键 (UP/DOWN/LEFT/RIGHT/ENTER/RETURN) 映射为
 * LVGL keypad 输入事件 (LV_KEY_UP/DOWN/LEFT/RIGHT/ENTER/ESC)。
 *
 * 数据流：
 *   GPIO按键 → TDL按键驱动 → button_callback → sg_current_key
 *   → LVGL keypad_read_cb → LV_EVENT_KEY → 页面事件处理
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_input.h"
#include "display_sleep.h"
#include "tdl_button_manage.h"
#include "board_com_api.h"
#include "tal_log.h"

#include <string.h>
#include <stdbool.h>

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/***********************************************************
 ********************* 类型定义 ****************************
 ***********************************************************/
typedef struct {
    char    *button_name;
    uint32_t lv_key;
} BUTTON_KEY_MAP_T;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/
static lv_indev_t *sg_keypad_indev = NULL;
static uint32_t    sg_current_key  = 0;

/* 按键名称 → LVGL 键码映射 */
static const BUTTON_KEY_MAP_T sg_button_key_map[] = {
    {BOARD_BUTTON_NAME_UP,     LV_KEY_UP},
    {BOARD_BUTTON_NAME_DOWN,   LV_KEY_DOWN},
    {BOARD_BUTTON_NAME_LEFT,   LV_KEY_LEFT},
    {BOARD_BUTTON_NAME_RIGHT,  LV_KEY_RIGHT},
    {BOARD_BUTTON_NAME_ENTER,  LV_KEY_ENTER},
    {BOARD_BUTTON_NAME_RETURN, LV_KEY_ESC},
};

#define BUTTON_MAP_COUNT (sizeof(sg_button_key_map) / sizeof(sg_button_key_map[0]))

/***********************************************************
 ********************* 按键回调 ****************************
 ***********************************************************/

/**
 * @brief 物理按键事件回调 — 按下时记录对应 LVGL 键码
 */
static void __button_callback(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    (void)argc;

    if (event == TDL_BUTTON_PRESS_DOWN) {
        for (uint32_t i = 0; i < BUTTON_MAP_COUNT; i++) {
            if (strcmp(name, sg_button_key_map[i].button_name) == 0) {
                sg_current_key = sg_button_key_map[i].lv_key;
                PR_DEBUG("[input] 按键 '%s' -> LV_KEY: %lu", name, sg_current_key);

                /* 重置空闲定时器 — 用户活动 */
                sleep_manager_activity();

                break;
            }
        }
    }
}

/***********************************************************
 ***************** LVGL keypad 读取回调 ********************
 ***********************************************************/

/**
 * @brief LVGL keypad 读取回调 — 返回当前按键状态
 */
static void __keypad_read_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;

    data->key = sg_current_key;

    if (sg_current_key != 0) {
        data->state    = LV_INDEV_STATE_PRESSED;
        sg_current_key = 0; /* 读取后清除，避免重复触发 */
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/***********************************************************
 ******************** 初始化入口 ***************************
 ***********************************************************/

OPERATE_RET display_input_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("[input] 初始化按键输入设备...");

    /* 1. 创建按键句柄并注册事件回调 */
    TDL_BUTTON_CFG_T button_cfg = {
        .long_start_valid_time     = 2000,
        .long_keep_timer           = 500,
        .button_debounce_time      = 50,
        .button_repeat_valid_count = 0,
        .button_repeat_valid_time  = 500,
    };

    for (uint32_t i = 0; i < BUTTON_MAP_COUNT; i++) {
        TDL_BUTTON_HANDLE handle = NULL;
        rt = tdl_button_create(sg_button_key_map[i].button_name, &button_cfg, &handle);
        if (rt != OPRT_OK) {
            PR_ERR("[input] 创建按键 '%s' 失败: %d", sg_button_key_map[i].button_name, rt);
            continue;
        }

        tdl_button_event_register(handle, TDL_BUTTON_PRESS_DOWN, __button_callback);
        tdl_button_event_register(handle, TDL_BUTTON_PRESS_UP, __button_callback);

        PR_NOTICE("[input] 按键 '%s' -> LV_KEY: %lu", sg_button_key_map[i].button_name,
                  sg_button_key_map[i].lv_key);
    }

    /* 2. 创建 LVGL keypad 输入设备 */
    sg_keypad_indev = lv_indev_create();
    if (sg_keypad_indev == NULL) {
        PR_ERR("[input] 创建 LVGL keypad 输入设备失败");
        return OPRT_MALLOC_FAILED;
    }

    lv_indev_set_type(sg_keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(sg_keypad_indev, __keypad_read_cb);

    /* 3. 创建默认组并绑定 */
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_indev_set_group(sg_keypad_indev, group);

    PR_NOTICE("[input] LVGL keypad 输入设备注册完成");
    return OPRT_OK;
}

lv_indev_t *display_input_get_keypad(void)
{
    return sg_keypad_indev;
}
