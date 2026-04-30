/**
 * @file display_sleep.c
 * @brief 休眠页面与深度休眠管理器实现
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_sleep.h"
#include "display_status_bar.h"
#include "display_i18n.h"

#include "tal_log.h"
#include "lv_vendor.h"
#include "tal_sw_timer.h"
#include "tal_system.h"
#include "tkl_wakeup.h"
#include "tkl_sleep.h"
#include "tuya_cloud_types.h"

#include <string.h>
#include <stdint.h>

/***********************************************************
 ********************* 宏定义 ****************************
 ***********************************************************/

/* 按键 GPIO 定义（来自 board driver） */
#define BOARD_BUTTON_UP_PIN           TUYA_GPIO_NUM_22
#define BOARD_BUTTON_DOWN_PIN         TUYA_GPIO_NUM_23
#define BOARD_BUTTON_ENTER_PIN        TUYA_GPIO_NUM_24
#define BOARD_BUTTON_RETURN_PIN       TUYA_GPIO_NUM_25
#define BOARD_BUTTON_LEFT_PIN         TUYA_GPIO_NUM_26
#define BOARD_BUTTON_RIGHT_PIN        TUYA_GPIO_NUM_28

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/

lv_obj_t *scr_sleep = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/

/* 空闲定时器句柄和超时时间 */
static TIMER_ID sg_idle_timer_id = NULL;
static uint32_t sg_timeout_ms     = 5 * 60 * 1000; /* 默认 5 分钟 */

/***********************************************************
 ********************* 深度休眠执行 ************************
 ***********************************************************/

/**
 * @brief 进入深度休眠 — 会立即执行，不返回（CPU 复位）
 */
static void __enter_deep_sleep(void)
{
    PR_NOTICE("[sleep] 进入深度休眠流程...");

    /* 1. LVGL lock 并加载休眠页面 */
    lv_vendor_disp_lock();

    if (scr_sleep == NULL) {
        display_sleep_init();
    }

    lv_screen_load_anim(scr_sleep, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    lv_refr_now(NULL); /* 立即刷新 */

    lv_vendor_disp_unlock();

    PR_NOTICE("[sleep] 休眠页面已显示，等待 EPD 完成全帧刷新...");

    /* 2. 等待 EPD 完成全帧刷新（UC8276 最慢需要 2-3 秒） */
    tal_system_sleep(4000);

    /* 3. 关背光 + 停止 LVGL 渲染（防止背光被重新打开） */
    lv_vendor_set_backlight(0);
    lv_vendor_stop();  /* 停止 LVGL 渲染任务 */
    PR_NOTICE("[sleep] 背光已关闭，LVGL 渲染已停止");

    /* 4. 配置按键 GPIO 为唤醒源（Active LOW 按下唤醒）
     * 注意：BK7258 最多只支持 4 个 GPIO 作为唤醒源
     * 保留：上（22）、下（23）、确认（24）、返回（25）, 左（26）和右（28）按键中优先配置前 4 个 GPIO 作为唤醒源
     * 
     */
    TUYA_GPIO_NUM_E button_pins[] = {
        BOARD_BUTTON_UP_PIN,      /* GPIO 22 */
        BOARD_BUTTON_DOWN_PIN,    /* GPIO 23 */
        BOARD_BUTTON_ENTER_PIN,   /* GPIO 24 */
        BOARD_BUTTON_RETURN_PIN,  /* GPIO 25 */
        BOARD_BUTTON_LEFT_PIN,
        BOARD_BUTTON_RIGHT_PIN,

    };

    for (size_t i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); i++) {
        TUYA_WAKEUP_SOURCE_BASE_CFG_T cfg = {0};
        cfg.source = TUYA_WAKEUP_SOURCE_GPIO;
        cfg.wakeup_para.gpio_param.gpio_num = button_pins[i];
        cfg.wakeup_para.gpio_param.level    = TUYA_GPIO_WAKEUP_FALL; /* Active LOW */

        if (tkl_wakeup_source_set(&cfg) == OPRT_OK) {
            PR_DEBUG("[sleep] 配置唤醒源: GPIO %d", button_pins[i]);
        }
    }

    PR_NOTICE("[sleep] 已配置 4 个唤醒源（GPIO 22/23/24/25），进入 CPU 深度休眠...");

    /* 5. 进入深度休眠（此后不返回 — CPU 复位） */
    tkl_cpu_sleep_mode_set(TRUE, TUYA_CPU_DEEP_SLEEP);

    /* 不会执行到这里 */
    PR_ERR("[sleep] ERROR: 深度休眠返回！");
}

/**
 * @brief 空闲定时器超时回调
 */
static void __sleep_timer_callback(TIMER_ID timer_id, void *arg)
{
    (void)timer_id;
    (void)arg;
    PR_WARN("[sleep] 空闲定时器超时，触发深度休眠");
    __enter_deep_sleep();
}

/***********************************************************
 ********************* 定时器管理 **************************
 ***********************************************************/

/**
 * @brief 重置空闲定时器 — 任何按键活动后调用
 */
void sleep_manager_activity(void)
{
    if (sg_idle_timer_id != NULL) {
        tal_sw_timer_stop(sg_idle_timer_id);
        PR_DEBUG("[sleep] 检测到用户活动，重置空闲定时器");
    }

    /* 重新启动定时器 */
    if (tal_sw_timer_start(sg_idle_timer_id, sg_timeout_ms, TAL_TIMER_ONCE) != OPRT_OK) {
        PR_ERR("[sleep] 空闲定时器启动失败");
    }
}

/**
 * @brief 设置休眠超时时间
 */
void sleep_manager_set_timeout(uint32_t minutes)
{
    if (minutes == 0 || minutes > 60) {
        PR_WARN("[sleep] 无效的超时时间: %u 分钟", minutes);
        return;
    }

    sg_timeout_ms = minutes * 60 * 1000;
    PR_NOTICE("[sleep] 休眠超时设置为: %u 分钟 (%u ms)", minutes, sg_timeout_ms);

    /* 立即重置定时器以应用新的超时时间 */
    sleep_manager_activity();
}

/***********************************************************
 ********************* 页面初始化 **************************
 ***********************************************************/

void display_sleep_init(void)
{
    if (scr_sleep != NULL) return;

    scr_sleep = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_sleep, lv_color_white(), 0);
    lv_obj_remove_flag(scr_sleep, LV_OBJ_FLAG_SCROLLABLE);

    /* 左上：AI */
    lv_obj_t *lbl_ai = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_ai, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_ai, lv_color_black(), 0);
    lv_label_set_text(lbl_ai, "AI");
    lv_obj_set_align(lbl_ai, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl_ai, 12, 12);

    /* 右上：菜单 */
    lv_obj_t *lbl_menu = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_menu, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_menu, lv_color_black(), 0);
    lv_label_set_text(lbl_menu, "菜单");
    lv_obj_set_align(lbl_menu, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl_menu, -12, 12);

    /* 中央：标题 */
    lv_obj_t *lbl_title = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_title, &font_puhui_16_2, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_black(), 0);
    lv_label_set_text(lbl_title, "T5 Eink Note");
    lv_obj_set_align(lbl_title, LV_ALIGN_CENTER);
    lv_obj_set_pos(lbl_title, 0, -40);

    /* 中央：副标题 */
    lv_obj_t *lbl_subtitle = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_subtitle, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_subtitle, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(lbl_subtitle, "Designed by Tuya DuckyClaw");
    lv_obj_set_align(lbl_subtitle, LV_ALIGN_CENTER);
    lv_obj_set_pos(lbl_subtitle, 0, -15);

    /* 底部：提示文本 */
    lv_obj_t *lbl_hint = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_hint, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_hint, lv_color_black(), 0);
    lv_label_set_text(lbl_hint, "设备已经休眠，按右侧任意按键唤醒");
    lv_obj_set_width(lbl_hint, 350);
    lv_label_set_long_mode(lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_align(lbl_hint, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(lbl_hint, 0, -25);

    /* 右侧：按键导航菜单 */
    /* 上键 */
    lv_obj_t *lbl_up = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_up, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_up, lv_color_black(), 0);
    lv_label_set_text(lbl_up, "上");
    lv_obj_set_align(lbl_up, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl_up, -25, 50);

    lv_obj_t *lbl_up_arrow = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_up_arrow, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_up_arrow, lv_color_black(), 0);
    lv_label_set_text(lbl_up_arrow, "→");
    lv_obj_set_align(lbl_up_arrow, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl_up_arrow, -5, 50);

    /* 下键 */
    lv_obj_t *lbl_down = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_down, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_down, lv_color_black(), 0);
    lv_label_set_text(lbl_down, "下");
    lv_obj_set_align(lbl_down, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl_down, -25, 100);

    lv_obj_t *lbl_down_arrow = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_down_arrow, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_down_arrow, lv_color_black(), 0);
    lv_label_set_text(lbl_down_arrow, "→");
    lv_obj_set_align(lbl_down_arrow, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl_down_arrow, -5, 100);

    /* 确认键 */
    lv_obj_t *lbl_enter = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_enter, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_enter, lv_color_black(), 0);
    lv_label_set_text(lbl_enter, "确认");
    lv_obj_set_align(lbl_enter, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl_enter, -35, -75);

    lv_obj_t *lbl_enter_arrow = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_enter_arrow, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_enter_arrow, lv_color_black(), 0);
    lv_label_set_text(lbl_enter_arrow, "→");
    lv_obj_set_align(lbl_enter_arrow, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl_enter_arrow, -5, -75);

    /* 返回键 */
    lv_obj_t *lbl_return = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_return, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_return, lv_color_black(), 0);
    lv_label_set_text(lbl_return, "返回");
    lv_obj_set_align(lbl_return, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl_return, -35, -25);

    lv_obj_t *lbl_return_arrow = lv_label_create(scr_sleep);
    lv_obj_set_style_text_font(lbl_return_arrow, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_return_arrow, lv_color_black(), 0);
    lv_label_set_text(lbl_return_arrow, "→");
    lv_obj_set_align(lbl_return_arrow, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl_return_arrow, -5, -25);

    PR_DEBUG("[sleep] 休眠页面初始化完成");
}

/**
 * @brief 初始化深度休眠管理器
 */
void sleep_manager_init(void)
{
    PR_NOTICE("[sleep] 初始化深度休眠管理器，默认超时: 5 分钟");

    /* 创建空闲定时器 */
    if (tal_sw_timer_create(__sleep_timer_callback, NULL, &sg_idle_timer_id) != OPRT_OK) {
        PR_ERR("[sleep] 空闲定时器创建失败!");
        return;
    }

    /* 启动定时器 */
    if (tal_sw_timer_start(sg_idle_timer_id, sg_timeout_ms, TAL_TIMER_ONCE) != OPRT_OK) {
        PR_ERR("[sleep] 空闲定时器启动失败!");
        return;
    }

    PR_NOTICE("[sleep] 空闲定时器已启动");
}
