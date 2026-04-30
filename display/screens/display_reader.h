/**
 * @file display_reader.h
 * @brief TXT 文件阅读器页面
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DISPLAY_READER_H__
#define __DISPLAY_READER_H__

#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 阅读器屏幕对象 */
extern lv_obj_t *scr_reader;

/**
 * @brief 设置要阅读的文件路径（在 init 之前调用）
 *
 * @param filepath SD 卡上的完整路径
 */
void display_reader_set_file(const char *filepath);

/**
 * @brief 初始化阅读器页面
 */
void display_reader_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_READER_H__ */
