/**
 * @file ducky_notice_history.h
 * @brief Ring buffer of recent cloud notice (DP101) bodies for the notice list screen
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef DUCKY_NOTICE_HISTORY_H
#define DUCKY_NOTICE_HISTORY_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef DUCKY_NOTICE_HIST_CAP
#define DUCKY_NOTICE_HIST_CAP 20
#endif

#ifndef DUCKY_NOTICE_HIST_TEXT_MAX
#define DUCKY_NOTICE_HIST_TEXT_MAX 384
#endif

/* ---------------------------------------------------------------------------
 * Function declarations
 * --------------------------------------------------------------------------- */
/**
 * @brief Append a notice body (newest). Safe from DP / non-LVGL thread.
 * @param[in] body Display text (same as popup body)
 * @return none
 */
void ducky_notice_history_push(const char *body);

/**
 * @brief Number of stored items (capped at DUCKY_NOTICE_HIST_CAP)
 * @return Count
 */
int ducky_notice_history_count(void);

/**
 * @brief Copy one history entry
 * @param[in] index_newest_first 0 = newest
 * @param[out] out Buffer
 * @param[in] out_sz Buffer size
 * @return OPRT_OK on success
 */
OPERATE_RET ducky_notice_history_get(int index_newest_first, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* DUCKY_NOTICE_HISTORY_H */
