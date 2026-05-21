/**
 * @file ducky_notice_popup.h
 * @brief Cloud DP101 notice: parse payload and show full-screen toast on LVGL
 * @version 1.0
 * @date 2026-03-27
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef DUCKY_NOTICE_POPUP_H
#define DUCKY_NOTICE_POPUP_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse DP101 string; if it describes a notice, queue a modal on the display.
 * @param[in] raw_dp_str string from dp->value.dp_str (pipe or JSON with items[])
 * @return none
 */
void ducky_notice_handle_dp_string(const char *raw_dp_str);

#ifdef __cplusplus
}
#endif

#endif /* DUCKY_NOTICE_POPUP_H */
