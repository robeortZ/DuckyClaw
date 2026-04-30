/**
 * @file display_reader.c
 * @brief TXT 文件阅读器页面 — 墨水屏电子书阅读器
 *
 * 布局：
 *   [状态栏]
 *   [文件名标题栏]
 *   [文本内容区域 - 分页显示]
 *   [页码: N/M]               (右下角)
 *
 * 按键：上/下滚动行，左/右翻页，ESC返回文件夹
 * 触摸：点击状态栏返回文件夹
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "display_reader.h"
#include "display_status_bar.h"
#include "display_folder.h"
#include "display_input.h"
#include "display_i18n.h"

#include "font_awesome_symbols.h"
#include "tal_log.h"
#include "tkl_fs.h"
#include "tal_memory.h"

#include <stdio.h>
#include <string.h>

/***********************************************************
 ********************* 宏定义 ******************************
 ***********************************************************/
#define READER_MAX_FILE_SIZE  (32 * 1024)  /* 最大读取 32KB */
#define READER_TITLE_HEIGHT   24           /* 标题栏高度 */
#define READER_CONTENT_TOP    (STATUS_BAR_HEIGHT + READER_TITLE_HEIGHT)
#define READER_CONTENT_HEIGHT (DISP_VER_RES - READER_CONTENT_TOP - 20)
#define READER_LINE_HEIGHT    18           /* 行高（14号字体） */
#define READER_LINES_PER_PAGE (READER_CONTENT_HEIGHT / READER_LINE_HEIGHT)
#define READER_PATH_MAX       256

/***********************************************************
 ********************* 全局变量 ****************************
 ***********************************************************/
lv_obj_t *scr_reader = NULL;

/***********************************************************
 ********************* 静态变量 ****************************
 ***********************************************************/
static char sg_filepath[READER_PATH_MAX] = {0};  /* 当前文件路径 */
static char *sg_file_buf  = NULL;  /* 文件内容缓冲 */
static int   sg_file_size = 0;

/* 页面偏移表 — 记录每页起始字节偏移 */
#define READER_MAX_PAGES 512
static int  sg_page_offsets[READER_MAX_PAGES];
static int  sg_total_pages = 0;
static int  sg_cur_page    = 0;

static lv_obj_t *sg_lbl_title   = NULL;  /* 文件名标题 */
static lv_obj_t *sg_lbl_content = NULL;  /* 文本内容标签 */
static lv_obj_t *sg_lbl_page    = NULL;  /* 页码标签 */
static lv_group_t *sg_group     = NULL;

/***********************************************************
 ******************* 文件读取 ******************************
 ***********************************************************/

/**
 * @brief 读取文件到缓冲区
 */
static bool __load_file(void)
{
    if (sg_file_buf) {
        tal_free(sg_file_buf);
        sg_file_buf = NULL;
    }
    sg_file_size = 0;

    int fsize = tkl_fgetsize(sg_filepath);
    if (fsize <= 0) {
        PR_WARN("[reader] 获取文件大小失败: %s", sg_filepath);
        return false;
    }

    /* 限制最大读取大小 */
    if (fsize > READER_MAX_FILE_SIZE) {
        fsize = READER_MAX_FILE_SIZE;
    }

    sg_file_buf = (char *)tal_malloc(fsize + 1);
    if (!sg_file_buf) {
        PR_ERR("[reader] 内存分配失败: %d bytes", fsize + 1);
        return false;
    }

    TUYA_FILE fp = tkl_fopen(sg_filepath, "r");
    if (!fp) {
        PR_WARN("[reader] 打开文件失败: %s", sg_filepath);
        tal_free(sg_file_buf);
        sg_file_buf = NULL;
        return false;
    }

    int nread = tkl_fread(sg_file_buf, fsize, fp);
    tkl_fclose(fp);

    if (nread <= 0) {
        PR_WARN("[reader] 读取文件失败: %s", sg_filepath);
        tal_free(sg_file_buf);
        sg_file_buf = NULL;
        return false;
    }

    sg_file_buf[nread] = '\0';
    sg_file_size = nread;

    PR_DEBUG("[reader] 读取文件成功: %s (%d bytes)", sg_filepath, sg_file_size);
    return true;
}

/***********************************************************
 ****************** 分页计算 *******************************
 ***********************************************************/

/**
 * @brief 判断字节是否是 UTF-8 多字节序列的起始字节
 */
static int __utf8_char_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; /* 无效字节当作 1 字节跳过 */
}

/**
 * @brief 计算分页偏移表
 *
 * 逐字符扫描文本，根据行宽和行数计算每页的起始偏移。
 * 中文字符约占 14px 宽，ASCII 约占 8px 宽。
 */
static void __calc_pages(void)
{
    sg_total_pages = 0;
    if (!sg_file_buf || sg_file_size <= 0) {
        sg_page_offsets[0] = 0;
        sg_total_pages = 1;
        return;
    }

    /* 内容区域可用宽度（减去左右边距） */
    const int content_width = DISP_HOR_RES - 16;
    const int char_width_cn = 14;  /* 中文字符宽度（14号字体） */
    const int char_width_en = 8;   /* ASCII 字符宽度 */

    int offset = 0;
    sg_page_offsets[0] = 0;
    int line_count = 0;
    int line_px = 0;

    while (offset < sg_file_size && sg_total_pages < READER_MAX_PAGES - 1) {
        unsigned char c = (unsigned char)sg_file_buf[offset];

        if (c == '\n') {
            /* 换行 */
            line_count++;
            line_px = 0;
            offset++;
            if (line_count >= READER_LINES_PER_PAGE) {
                sg_total_pages++;
                sg_page_offsets[sg_total_pages] = offset;
                line_count = 0;
            }
            continue;
        }

        if (c == '\r') {
            offset++;
            continue;
        }

        int clen = __utf8_char_len(c);
        int cpx = (clen >= 3) ? char_width_cn : char_width_en;

        if (line_px + cpx > content_width) {
            /* 自动换行 */
            line_count++;
            line_px = 0;
            if (line_count >= READER_LINES_PER_PAGE) {
                sg_total_pages++;
                sg_page_offsets[sg_total_pages] = offset;
                line_count = 0;
            }
        }

        line_px += cpx;
        offset += clen;
    }

    /* 最后一页 */
    sg_total_pages++;

    PR_DEBUG("[reader] 共 %d 页", sg_total_pages);
}

/***********************************************************
 ****************** 页面显示 *******************************
 ***********************************************************/

/**
 * @brief 显示当前页内容
 */
static void __show_page(void)
{
    if (!sg_lbl_content || !sg_file_buf) {
        return;
    }

    int start = sg_page_offsets[sg_cur_page];
    int end;
    if (sg_cur_page + 1 < sg_total_pages) {
        end = sg_page_offsets[sg_cur_page + 1];
    } else {
        end = sg_file_size;
    }

    /* 临时截断字符串显示当前页 */
    char saved = sg_file_buf[end];
    sg_file_buf[end] = '\0';
    lv_label_set_text(sg_lbl_content, &sg_file_buf[start]);
    sg_file_buf[end] = saved;

    /* 更新页码 */
    char page_buf[16];
    snprintf(page_buf, sizeof(page_buf), "%d/%d", sg_cur_page + 1, sg_total_pages);
    lv_label_set_text(sg_lbl_page, page_buf);
}

/***********************************************************
 ****************** 按键事件 *******************************
 ***********************************************************/

static void __go_back(void)
{
    display_screen_change(&scr_folder, display_folder_init);
}

static void __status_bar_click_cb(lv_event_t *e)
{
    (void)e;
    __go_back();
}

static void __screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED) {
        sg_group = lv_group_create();
        lv_group_add_obj(sg_group, scr_reader);
        lv_indev_t *indev = display_input_get_keypad();
        if (indev) lv_indev_set_group(indev, sg_group);

        /* 加载文件并显示第一页 */
        if (__load_file()) {
            __calc_pages();
            sg_cur_page = 0;
            __show_page();
        } else {
            lv_label_set_text(sg_lbl_content, i18n_str(STR_OPEN_FAIL));
            lv_label_set_text(sg_lbl_page, "0/0");
        }
    } else if (code == LV_EVENT_KEY) {
        lv_key_t key = lv_event_get_key(e);
        switch (key) {
        case LV_KEY_UP:
        case LV_KEY_LEFT:
            /* 上一页 */
            if (sg_cur_page > 0) {
                sg_cur_page--;
                __show_page();
            }
            break;
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
            /* 下一页 */
            if (sg_cur_page + 1 < sg_total_pages) {
                sg_cur_page++;
                __show_page();
            }
            break;
        case LV_KEY_ESC:
            __go_back();
            break;
        default:
            break;
        }
    }
}

/***********************************************************
 ******************** 初始化 *******************************
 ***********************************************************/

void display_reader_set_file(const char *filepath)
{
    strncpy(sg_filepath, filepath, READER_PATH_MAX - 1);
    sg_filepath[READER_PATH_MAX - 1] = '\0';
}

void display_reader_init(void)
{
    if (scr_reader) {
        /* 重新进入时需要销毁旧屏幕（文件可能不同） */
        lv_obj_delete(scr_reader);
        scr_reader = NULL;
        if (sg_file_buf) {
            tal_free(sg_file_buf);
            sg_file_buf = NULL;
        }
    }

    scr_reader = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_reader, lv_color_white(), 0);
    lv_obj_remove_flag(scr_reader, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏（点击可返回文件夹） */
    lv_obj_t *bar = status_bar_create(scr_reader, i18n_str(STR_READER));
    lv_obj_add_event_cb(bar, __status_bar_click_cb, LV_EVENT_CLICKED, NULL);

    /* 文件名标题栏 */
    sg_lbl_title = lv_label_create(scr_reader);
    lv_obj_set_style_text_font(sg_lbl_title, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_title, lv_color_make(0x40, 0x40, 0x40), 0);
    lv_obj_set_width(sg_lbl_title, DISP_HOR_RES - 24);
    lv_label_set_long_mode(sg_lbl_title, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(sg_lbl_title, 8, STATUS_BAR_HEIGHT + 2);

    /* 从路径中提取文件名显示 */
    const char *fname = strrchr(sg_filepath, '/');
    lv_label_set_text(sg_lbl_title, fname ? (fname + 1) : sg_filepath);

    /* 文本内容区域 */
    sg_lbl_content = lv_label_create(scr_reader);
    lv_obj_set_style_text_font(sg_lbl_content, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_content, lv_color_black(), 0);
    lv_obj_set_size(sg_lbl_content, DISP_HOR_RES - 16, READER_CONTENT_HEIGHT);
    lv_obj_set_pos(sg_lbl_content, 8, READER_CONTENT_TOP);
    lv_label_set_long_mode(sg_lbl_content, LV_LABEL_LONG_WRAP);
    lv_label_set_text(sg_lbl_content, "");

    /* 页码标签（右下角） */
    sg_lbl_page = lv_label_create(scr_reader);
    lv_obj_set_style_text_font(sg_lbl_page, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(sg_lbl_page, lv_color_make(0x60, 0x60, 0x60), 0);
    lv_label_set_text(sg_lbl_page, "0/0");
    lv_obj_set_align(sg_lbl_page, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(sg_lbl_page, -12, -4);

    /* 注册屏幕事件 */
    lv_obj_add_event_cb(scr_reader, __screen_event_cb, LV_EVENT_ALL, NULL);

    PR_DEBUG("[display] 阅读器页面初始化完成: %s", sg_filepath);
}
