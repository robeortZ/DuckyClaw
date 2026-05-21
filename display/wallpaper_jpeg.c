/**
 * @file wallpaper_jpeg.c
 * @brief Home wallpaper: PNG under /sdcard/image (or fallback), file bytes from claw_malloc for less internal RAM
 * @version 1.0
 * @date 2026-03-27
 * @copyright Copyright (c) Tuya Inc.
 */
#include "wallpaper_jpeg.h"

#include "tool_files.h"

#include "tal_log.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_T5) && (PLATFORM_T5 == 1)
#include "tkl_fs.h"
#endif

#if defined(PLATFORM_T5) && (PLATFORM_T5 == 1) && LV_USE_LODEPNG && defined(CLAW_USE_SDCARD) && (CLAW_USE_SDCARD == 1)

/**
 * @brief ASCII to lowercase for case-insensitive suffix match
 * @param[in] c input byte
 * @return lowercase or unchanged
 */
static int __lower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

/**
 * @brief Case-insensitive suffix test
 * @param[in] name filename or path tail
 * @param[in] suffix suffix to match (e.g. ".png")
 * @return 1 if name ends with suffix
 */
static int __str_ends_ci(const char *name, const char *suffix)
{
    size_t nl = strlen(name);
    size_t sl = strlen(suffix);
    size_t i;
    if (nl < sl) {
        return 0;
    }
    for (i = 0; i < sl; i++) {
        if (__lower((unsigned char)name[nl - sl + i]) != __lower((unsigned char)suffix[i])) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Whether filename is .png (case-insensitive)
 * @param[in] name directory entry name
 * @return 1 if PNG
 */
static int __is_png_name(const char *name)
{
    return __str_ends_ci(name, ".png");
}

/**
 * @brief Stable order: first PNG is lexically first path under directory
 * @param[in,out] list wallpaper list
 * @return none
 */
static void __sort_wallpaper_paths(wallpaper_list_t *list)
{
    int  i;
    int  j;
    char tmp[DUCKY_WALLPAPER_PATH_MAX];

    for (i = 0; i < list->count - 1; i++) {
        for (j = i + 1; j < list->count; j++) {
            if (strcmp(list->paths[i], list->paths[j]) > 0) {
                memcpy(tmp, list->paths[i], sizeof(tmp));
                memcpy(list->paths[i], list->paths[j], sizeof(tmp));
                memcpy(list->paths[j], tmp, sizeof(tmp));
            }
        }
    }
}

/**
 * @brief Append sorted PNG paths from one directory to list
 * @param[in] dir absolute directory path
 * @param[in,out] list receives paths; count may grow
 * @return OPRT_OK if at least one PNG found
 */
static OPERATE_RET __scan_one_dir_png(const char *dir, wallpaper_list_t *list)
{
    TUYA_DIR       d      = NULL;
    TUYA_FILEINFO  info   = NULL;
    BOOL_T         is_reg = 0;
    BOOL_T         is_dir = 0;
    CONST CHAR_T * name   = NULL;
    INT_T          ir;

    if (!dir || !list) {
        return OPRT_INVALID_PARM;
    }

    ir = tkl_dir_open(dir, &d);
    if (ir != 0 || d == NULL) {
        return OPRT_COM_ERROR;
    }

    while (tkl_dir_read(d, &info) == 0 && info != NULL) {
        if (tkl_dir_is_directory(info, &is_dir) != 0) {
            continue;
        }
        if (is_dir) {
            continue;
        }
        if (tkl_dir_is_regular(info, &is_reg) != 0 || !is_reg) {
            continue;
        }
        if (tkl_dir_name(info, &name) != 0 || name == NULL) {
            continue;
        }
        if (!__is_png_name(name)) {
            continue;
        }
        if (list->count >= DUCKY_WALLPAPER_MAX_FILES) {
            break;
        }
        (void)snprintf(list->paths[list->count], DUCKY_WALLPAPER_PATH_MAX, "%s/%s", dir, name);
        list->count++;
    }

    tkl_dir_close(d);
    if (list->count <= 0) {
        return OPRT_NOT_FOUND;
    }
    __sort_wallpaper_paths(list);
    return OPRT_OK;
}

#endif /* T5 + LODEPNG + SD */

/**
 * @brief Legacy hook; JPEG decoder removed
 * @return OPRT_OK
 */
OPERATE_RET wallpaper_jpeg_codec_init(void)
{
    return OPRT_OK;
}

/**
 * @brief Scan SD `image` directories for PNGs (see wallpaper_jpeg.h)
 * @param[in,out] list output list
 * @return OPRT_OK if any file found
 */
OPERATE_RET wallpaper_scan_directory(wallpaper_list_t *list)
{
    if (!list) {
        return OPRT_INVALID_PARM;
    }
    memset(list, 0, sizeof(*list));
    list->index = 0;

#if defined(PLATFORM_T5) && (PLATFORM_T5 == 1) && LV_USE_LODEPNG && defined(CLAW_USE_SDCARD) && (CLAW_USE_SDCARD == 1)
    if (__scan_one_dir_png(DUCKY_WALLPAPER_DIR_PRIMARY, list) == OPRT_OK) {
        return OPRT_OK;
    }
    list->count = 0;
    if (__scan_one_dir_png(DUCKY_WALLPAPER_DIR_FALLBACK, list) == OPRT_OK) {
        return OPRT_OK;
    }
    return OPRT_NOT_FOUND;
#else
    return OPRT_NOT_SUPPORTED;
#endif
}

/**
 * @brief Load PNG file bytes into dsc for LVGL lodepng (RAW)
 * @param[in] list populated list
 * @param[in] idx index or negative wrap
 * @param[out] out_dsc image descriptor; use wallpaper_free_decoded
 * @return OPRT_OK on success
 */
OPERATE_RET wallpaper_load_index(wallpaper_list_t *list, int idx, lv_image_dsc_t *out_dsc)
{
#if defined(PLATFORM_T5) && (PLATFORM_T5 == 1) && LV_USE_LODEPNG && defined(CLAW_USE_SDCARD) && (CLAW_USE_SDCARD == 1)
    TUYA_FILE      f     = NULL;
    INT_T          fsz   = 0;
    UINT8_T       *buf   = NULL;
    INT_T          nread = 0;
    static const uint8_t png_magic[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

    if (!list || !out_dsc || list->count <= 0) {
        return OPRT_INVALID_PARM;
    }
    if (idx < 0) {
        idx = list->count + idx;
    }
    if (idx < 0 || idx >= list->count) {
        return OPRT_INVALID_PARM;
    }

    memset(out_dsc, 0, sizeof(*out_dsc));

    fsz = tkl_fgetsize(list->paths[idx]);
    if (fsz <= 0 || fsz > (512 * 1024)) {
        PR_ERR("wallpaper png bad size %d", fsz);
        return OPRT_COM_ERROR;
    }

    buf = (UINT8_T *)claw_malloc((size_t)fsz);
    if (!buf) {
        return OPRT_MALLOC_FAILED;
    }

    f = tkl_fopen(list->paths[idx], "r");
    if (!f) {
        claw_free(buf);
        return OPRT_COM_ERROR;
    }
    nread = tkl_fread(buf, fsz, f);
    tkl_fclose(f);
    if (nread != fsz) {
        claw_free(buf);
        return OPRT_COM_ERROR;
    }

    if (memcmp(buf, png_magic, sizeof(png_magic)) != 0) {
        claw_free(buf);
        return OPRT_COM_ERROR;
    }

    out_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    out_dsc->header.cf    = LV_COLOR_FORMAT_RAW;
    out_dsc->header.w     = 0;
    out_dsc->header.h     = 0;
    out_dsc->data         = buf;
    out_dsc->data_size    = (uint32_t)fsz;

    return OPRT_OK;
#else
    (void)list;
    (void)idx;
    (void)out_dsc;
    return OPRT_NOT_SUPPORTED;
#endif
}

/**
 * @brief Release buffer from wallpaper_load_index
 * @param[in,out] dsc cleared after free
 * @return none
 */
void wallpaper_free_decoded(lv_image_dsc_t *dsc)
{
    if (!dsc) {
        return;
    }
    if (dsc->data) {
        claw_free((void *)dsc->data);
        dsc->data = NULL;
    }
    memset(dsc, 0, sizeof(*dsc));
}
