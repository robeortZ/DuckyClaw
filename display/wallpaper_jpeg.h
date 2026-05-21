/**
 * @file wallpaper_jpeg.h
 * @brief Home wallpaper: PNG files under SD `image` directory (RAW buffer for LVGL lodepng)
 * @version 1.0
 * @date 2026-03-27
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef WALLPAPER_JPEG_H
#define WALLPAPER_JPEG_H

#include "tuya_cloud_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** First try SD card (see CLAW_FS_ROOT_PATH in tool_files.h). */
#ifndef DUCKY_WALLPAPER_DIR_PRIMARY
#define DUCKY_WALLPAPER_DIR_PRIMARY "/sdcard/image"
#endif
#ifndef DUCKY_WALLPAPER_DIR_FALLBACK
#define DUCKY_WALLPAPER_DIR_FALLBACK "/t5_fs/image"
#endif

#define DUCKY_WALLPAPER_MAX_FILES 32
#define DUCKY_WALLPAPER_PATH_MAX  128

typedef struct {
    char paths[DUCKY_WALLPAPER_MAX_FILES][DUCKY_WALLPAPER_PATH_MAX];
    int  count;
    int  index;
} wallpaper_list_t;

/**
 * @brief Legacy no-op (JPEG codec removed); kept for call-site compatibility.
 */
OPERATE_RET wallpaper_jpeg_codec_init(void);

/**
 * @brief Fill list with .png files from primary then fallback directory (sorted by path).
 */
OPERATE_RET wallpaper_scan_directory(wallpaper_list_t *list);

/**
 * @brief Load PNG file at index into image dsc (LV_COLOR_FORMAT_RAW). Buffer from claw_malloc / claw_free.
 */
OPERATE_RET wallpaper_load_index(wallpaper_list_t *list, int idx, lv_image_dsc_t *out_dsc);

/**
 * @brief Free file buffer allocated by wallpaper_load_index.
 */
void wallpaper_free_decoded(lv_image_dsc_t *dsc);

#ifdef __cplusplus
}
#endif

#endif /* WALLPAPER_JPEG_H */
