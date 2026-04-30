/**
 * @file display_folder.c
 * @brief 文件夹浏览页面
 *
 * 显示 SD 卡目录文件列表，支持分页浏览、进入子目录、打开 txt 文件。
 * 按键：上/下移动焦点，左/右翻页，回车进入/打开，ESC返回上级
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_folder.h"
#include "display_status_bar.h"
#include "display_home.h"
#include "display_input.h"
#include "display_reader.h"

#include "display_i18n.h"
#include "font_awesome_symbols.h"
#include "tal_log.h"
#include "tkl_fs.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/***********************************************************
 ********************* 宏定义 ******************************
 ***********************************************************/
#define FOLDER_ROOT_PATH      "/sdcard"
#define FOLDER_PATH_MAX       256
#define FOLDER_ITEM_HEIGHT    44
#define FOLDER_ITEMS_PER_PAGE 5
#define FOLDER_NAME_MAX       64

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/
lv_obj_t *scr_folder = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/

/* 文件列表缓存 */
typedef struct {
    char name[FOLDER_NAME_MAX];
    bool is_dir;
} folder_entry_t;

static folder_entry_t sg_entries[64];
static int            sg_entry_count = 0;

/* 当前浏览路径 */
static char sg_cur_path[FOLDER_PATH_MAX] = FOLDER_ROOT_PATH;

static lv_obj_t *sg_list_container = NULL;
static lv_obj_t *sg_lbl_title     = NULL;  /* 路径标题 */
static lv_obj_t *sg_items[FOLDER_ITEMS_PER_PAGE] = {NULL};
static lv_obj_t *sg_lbl_page = NULL;
static int32_t   sg_focus_idx  = -1;
static uint32_t  sg_cur_page   = 0;
static uint32_t  sg_total_pages = 0;
static lv_group_t *sg_group    = NULL;

/***********************************************************
 ****************** 工具函数 *******************************
 ***********************************************************/

/**
 * @brief 判断文件名是否以 .txt 结尾（不区分大小写）
 */
static bool __is_txt_file(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + len - 4;
    return (ext[0] == '.' &&
            (ext[1] == 't' || ext[1] == 'T') &&
            (ext[2] == 'x' || ext[2] == 'X') &&
            (ext[3] == 't' || ext[3] == 'T'));
}

/**
 * @brief 判断是否在根目录
 */
static bool __is_root(void)
{
    return (strcmp(sg_cur_path, FOLDER_ROOT_PATH) == 0);
}

/***********************************************************
 ****************** 文件列表读取 ***************************
 ***********************************************************/

static void __scan_directory(void)
{
    sg_entry_count = 0;

    TUYA_DIR dir = NULL;
    TUYA_FILEINFO info = NULL;

    if (tkl_dir_open(sg_cur_path, &dir) != 0) {
        PR_WARN("[display] 打开目录失败: %s", sg_cur_path);
        return;
    }

    while (sg_entry_count < 64) {
        if (tkl_dir_read(dir, &info) != 0) {
            break;
        }
        const char *name = NULL;
        tkl_dir_name(info, &name);
        if (!name || name[0] == '.') {
            continue; /* 跳过隐藏文件 */
        }
        strncpy(sg_entries[sg_entry_count].name, name, FOLDER_NAME_MAX - 1);
        sg_entries[sg_entry_count].name[FOLDER_NAME_MAX - 1] = '\0';
        BOOL_T is_dir = FALSE;
        tkl_dir_is_directory(info, &is_dir);
        sg_entries[sg_entry_count].is_dir = is_dir;
        sg_entry_count++;
    }

    tkl_dir_close(dir);
    PR_DEBUG("[display] 扫描 %s: %d 个文件/目录", sg_cur_path, sg_entry_count);
}

/***********************************************************
 ****************** 列表项创建/更新 ************************
 ***********************************************************/

static lv_obj_t *__create_folder_item(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), FOLDER_ITEM_HEIGHT);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_100, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(row, lv_color_make(0xC0, 0xC0, 0xC0), 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);

    /* 图标 (child 0) */
    lv_obj_t *lbl_icon = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_icon, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_black(), 0);
    lv_label_set_text(lbl_icon, FONT_AWESOME_SD_CARD);
    lv_obj_set_align(lbl_icon, LV_ALIGN_LEFT_MID);

    /* 文件名 (child 1) */
    lv_obj_t *lbl_name = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_name, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_black(), 0);
    lv_label_set_text(lbl_name, "");
    lv_obj_set_align(lbl_name, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_name, 28, 0);
    lv_obj_set_width(lbl_name, 300);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_CLIP);

    /* 右箭头（目录用，child 2） */
    lv_obj_t *lbl_arrow = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_arrow, &font_awesome_16_4, 0);
    lv_obj_set_style_text_color(lbl_arrow, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_label_set_text(lbl_arrow, "");
    lv_obj_set_align(lbl_arrow, LV_ALIGN_RIGHT_MID);

    return row;
}

static void __update_folder_item(lv_obj_t *row, const folder_entry_t *entry)
{
    if (!row || !entry) {
        return;
    }
    lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);

    /* 图标：目录用文件夹图标，文件用 SD 卡图标 */
    lv_obj_t *lbl_icon = lv_obj_get_child(row, 0);
    lv_label_set_text(lbl_icon, entry->is_dir ? FONT_AWESOME_HOME : FONT_AWESOME_SD_CARD);

    /* 文件名 */
    lv_obj_t *lbl_name = lv_obj_get_child(row, 1);
    lv_label_set_text(lbl_name, entry->name);

    /* 右箭头：目录显示 >，文件不显示 */
    lv_obj_t *lbl_arrow = lv_obj_get_child(row, 2);
    lv_label_set_text(lbl_arrow, entry->is_dir ? FONT_AWESOME_ARROW_RIGHT : "");
}

static void __refresh_list(void)
{
    sg_total_pages = (sg_entry_count > 0) ?
        ((sg_entry_count + FOLDER_ITEMS_PER_PAGE - 1) / FOLDER_ITEMS_PER_PAGE) : 1;
    if (sg_cur_page >= sg_total_pages) {
        sg_cur_page = sg_total_pages - 1;
    }

    uint32_t start = sg_cur_page * FOLDER_ITEMS_PER_PAGE;
    for (int i = 0; i < FOLDER_ITEMS_PER_PAGE; i++) {
        uint32_t idx = start + i;
        if (idx < (uint32_t)sg_entry_count) {
            __update_folder_item(sg_items[i], &sg_entries[idx]);
        } else {
            lv_obj_add_flag(sg_items[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    char page_buf[16];
    snprintf(page_buf, sizeof(page_buf), "%u-%u", sg_cur_page + 1, sg_total_pages);
    lv_label_set_text(sg_lbl_page, page_buf);

    /* 更新路径标题 */
    if (sg_lbl_title) {
        lv_label_set_text(sg_lbl_title, sg_cur_path);
    }
}

/***********************************************************
 ****************** 焦点管理 *******************************
 ***********************************************************/

static void __set_item_focus(int32_t idx, bool focused)
{
    if (idx < 0 || idx >= FOLDER_ITEMS_PER_PAGE || !sg_items[idx]) return;
    if (lv_obj_has_flag(sg_items[idx], LV_OBJ_FLAG_HIDDEN)) return;

    if (focused) {
        lv_obj_set_style_border_width(sg_items[idx], 2, 0);
        lv_obj_set_style_border_color(sg_items[idx], lv_color_black(), 0);
        lv_obj_set_style_border_side(sg_items[idx], LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_radius(sg_items[idx], 4, 0);
    } else {
        lv_obj_set_style_border_width(sg_items[idx], 1, 0);
        lv_obj_set_style_border_color(sg_items[idx], lv_color_make(0xC0, 0xC0, 0xC0), 0);
        lv_obj_set_style_border_side(sg_items[idx], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_radius(sg_items[idx], 0, 0);
    }
}

static void __focus_move(int32_t delta)
{
    int visible = 0;
    for (int i = 0; i < FOLDER_ITEMS_PER_PAGE; i++) {
        if (sg_items[i] && !lv_obj_has_flag(sg_items[i], LV_OBJ_FLAG_HIDDEN)) visible++;
    }
    if (visible == 0) return;

    if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
    sg_focus_idx += delta;
    if (sg_focus_idx >= visible) sg_focus_idx = 0;
    if (sg_focus_idx < 0) sg_focus_idx = visible - 1;
    __set_item_focus(sg_focus_idx, true);
}

/**
 * @brief 安全版焦点移动（用于目录切换后设置初始焦点）
 */
static void __focus_move_safe(int32_t delta)
{
    int visible = 0;
    for (int i = 0; i < FOLDER_ITEMS_PER_PAGE; i++) {
        if (sg_items[i] && !lv_obj_has_flag(sg_items[i], LV_OBJ_FLAG_HIDDEN)) visible++;
    }
    if (visible == 0) return;

    sg_focus_idx = -1;
    sg_focus_idx += delta;
    if (sg_focus_idx >= visible) sg_focus_idx = 0;
    if (sg_focus_idx < 0) sg_focus_idx = visible - 1;
    __set_item_focus(sg_focus_idx, true);
}

/***********************************************************
 ****************** 目录导航 *******************************
 ***********************************************************/

/**
 * @brief 进入子目录
 */
static void __enter_directory(const char *dirname)
{
    size_t cur_len = strlen(sg_cur_path);
    size_t name_len = strlen(dirname);

    if (cur_len + 1 + name_len >= FOLDER_PATH_MAX) {
        PR_WARN("[display] 路径过长，无法进入: %s", dirname);
        return;
    }

    /* 拼接路径 */
    snprintf(sg_cur_path + cur_len, FOLDER_PATH_MAX - cur_len, "/%s", dirname);

    /* 重新扫描并刷新 */
    __scan_directory();
    sg_cur_page = 0;
    sg_focus_idx = -1;
    __refresh_list();
}

/**
 * @brief 返回上级目录
 *
 * @return true 成功返回上级，false 已在根目录
 */
static bool __go_parent(void)
{
    if (__is_root()) {
        return false;
    }

    /* 去掉最后一个 '/' 及其后面的内容 */
    char *last_slash = strrchr(sg_cur_path, '/');
    if (last_slash && last_slash != sg_cur_path) {
        *last_slash = '\0';
    }

    /* 防止路径比根目录还短 */
    if (strlen(sg_cur_path) < strlen(FOLDER_ROOT_PATH)) {
        strncpy(sg_cur_path, FOLDER_ROOT_PATH, FOLDER_PATH_MAX);
    }

    __scan_directory();
    sg_cur_page = 0;
    sg_focus_idx = -1;
    __refresh_list();
    return true;
}

/**
 * @brief 打开选中的文件或目录
 */
static void __open_selected(void)
{
    if (sg_focus_idx < 0) return;

    uint32_t entry_idx = sg_cur_page * FOLDER_ITEMS_PER_PAGE + sg_focus_idx;
    if (entry_idx >= (uint32_t)sg_entry_count) return;

    folder_entry_t *entry = &sg_entries[entry_idx];

    if (entry->is_dir) {
        /* 进入子目录 */
        __set_item_focus(sg_focus_idx, false);
        __enter_directory(entry->name);
        __focus_move_safe(1);
    } else if (__is_txt_file(entry->name)) {
        /* 打开 txt 文件 */
        char filepath[FOLDER_PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", sg_cur_path, entry->name);
        display_reader_set_file(filepath);
        /* 强制重新创建阅读器（文件不同） */
        scr_reader = NULL;
        display_screen_change(&scr_reader, display_reader_init);
    }
}

/***********************************************************
 ****************** 触摸点击事件 ***************************
 ***********************************************************/

/**
 * @brief 列表项触摸点击回调
 */
static void __item_click_cb(lv_event_t *e)
{
    int32_t idx = (int32_t)(intptr_t)lv_event_get_user_data(e);

    /* 更新焦点 */
    if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
    sg_focus_idx = idx;
    __set_item_focus(sg_focus_idx, true);

    /* 打开选中项 */
    __open_selected();
}

/***********************************************************
 ****************** 按键事件 *******************************
 ***********************************************************/

static void __screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED) {
        sg_group = lv_group_create();
        lv_group_add_obj(sg_group, scr_folder);
        lv_indev_t *indev = display_input_get_keypad();
        if (indev) lv_indev_set_group(indev, sg_group);

        __scan_directory();
        sg_cur_page = 0;
        __refresh_list();
        sg_focus_idx = -1;
        __focus_move(1);
    } else if (code == LV_EVENT_KEY) {
        lv_key_t key = lv_event_get_key(e);
        switch (key) {
        case LV_KEY_UP:
            __focus_move(-1);
            break;
        case LV_KEY_DOWN:
            __focus_move(1);
            break;
        case LV_KEY_LEFT:
            if (sg_cur_page > 0) {
                if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
                sg_cur_page--;
                __refresh_list();
                sg_focus_idx = 0;
                __set_item_focus(sg_focus_idx, true);
            }
            break;
        case LV_KEY_RIGHT:
            if (sg_cur_page + 1 < sg_total_pages) {
                if (sg_focus_idx >= 0) __set_item_focus(sg_focus_idx, false);
                sg_cur_page++;
                __refresh_list();
                sg_focus_idx = 0;
                __set_item_focus(sg_focus_idx, true);
            }
            break;
        case LV_KEY_ENTER:
            __open_selected();
            break;
        case LV_KEY_ESC:
            /* 先尝试返回上级目录，已在根目录则返回主页 */
            if (!__go_parent()) {
                display_screen_change(&scr_home, display_home_init);
            } else {
                __focus_move_safe(1);
            }
            break;
        default:
            break;
        }
    }
}

/**
 * @brief 状态栏点击回调 — 触摸返回主页
 */
static void __status_bar_click_cb(lv_event_t *e)
{
    (void)e;
    display_screen_change(&scr_home, display_home_init);
}

/***********************************************************
 ******************** 初始化 *******************************
 ***********************************************************/

void display_folder_init(void)
{
    if (scr_folder) return;

    /* 初始化时重置路径到根目录 */
    strncpy(sg_cur_path, FOLDER_ROOT_PATH, FOLDER_PATH_MAX);

    scr_folder = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_folder, lv_color_white(), 0);
    lv_obj_remove_flag(scr_folder, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏（点击可返回主页） */
    lv_obj_t *bar = status_bar_create(scr_folder, i18n_str(STR_FOLDER));
    lv_obj_add_event_cb(bar, __status_bar_click_cb, LV_EVENT_CLICKED, NULL);

    /* 路径标题栏 */
    sg_lbl_title = lv_label_create(scr_folder);
    lv_obj_set_style_text_font(sg_lbl_title, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_title, lv_color_make(0x40, 0x40, 0x40), 0);
    lv_obj_set_width(sg_lbl_title, DISP_HOR_RES - 16);
    lv_label_set_long_mode(sg_lbl_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(sg_lbl_title, 8, STATUS_BAR_HEIGHT + 2);
    lv_label_set_text(sg_lbl_title, sg_cur_path);

    /* 列表容器（标题下方） */
    sg_list_container = lv_obj_create(scr_folder);
    lv_obj_set_size(sg_list_container, DISP_HOR_RES, CONTENT_HEIGHT - 40);
    lv_obj_set_pos(sg_list_container, 0, STATUS_BAR_HEIGHT + 20);
    lv_obj_set_style_pad_all(sg_list_container, 0, 0);
    lv_obj_set_style_bg_opa(sg_list_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(sg_list_container, 0, 0);
    lv_obj_set_style_radius(sg_list_container, 0, 0);
    lv_obj_set_flex_flow(sg_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(sg_list_container, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < FOLDER_ITEMS_PER_PAGE; i++) {
        sg_items[i] = __create_folder_item(sg_list_container);
    }

    /* 注册触摸点击事件 */
    for (int i = 0; i < FOLDER_ITEMS_PER_PAGE; i++) {
        lv_obj_add_event_cb(sg_items[i], __item_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    /* 页码 */
    sg_lbl_page = lv_label_create(scr_folder);
    lv_obj_set_style_text_font(sg_lbl_page, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_page, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(sg_lbl_page, "1-1");
    lv_obj_set_align(sg_lbl_page, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(sg_lbl_page, -12, -4);

    lv_obj_add_event_cb(scr_folder, __screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 文件夹页面初始化完成");
}
