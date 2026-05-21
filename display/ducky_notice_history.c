/**
 * @file ducky_notice_history.c
 * @brief Ring buffer of recent notice bodies
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ducky_notice_history.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static char s_items[DUCKY_NOTICE_HIST_CAP][DUCKY_NOTICE_HIST_TEXT_MAX];
static int  s_head;
static int  s_count;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Append a notice body (newest)
 * @param[in] body Display text
 * @return none
 */
void ducky_notice_history_push(const char *body)
{
    if (!body || !body[0]) {
        return;
    }
    (void)snprintf(s_items[s_head], sizeof(s_items[s_head]), "%s", body);
    s_items[s_head][sizeof(s_items[s_head]) - 1] = '\0';
    s_head                                       = (s_head + 1) % DUCKY_NOTICE_HIST_CAP;
    if (s_count < DUCKY_NOTICE_HIST_CAP) {
        s_count++;
    }
}

/**
 * @brief Number of stored items
 * @return Count
 */
int ducky_notice_history_count(void)
{
    return s_count;
}

/**
 * @brief Copy one history entry
 * @param[in] index_newest_first 0 = newest
 * @param[out] out Buffer
 * @param[in] out_sz Buffer size
 * @return OPRT_OK on success
 */
OPERATE_RET ducky_notice_history_get(int index_newest_first, char *out, size_t out_sz)
{
    int ring_idx;

    if (!out || out_sz == 0) {
        return OPRT_INVALID_PARM;
    }
    if (index_newest_first < 0 || index_newest_first >= s_count) {
        return OPRT_INVALID_PARM;
    }
    ring_idx = (s_head - 1 - index_newest_first + (DUCKY_NOTICE_HIST_CAP * 16)) % DUCKY_NOTICE_HIST_CAP;
    (void)snprintf(out, out_sz, "%s", s_items[ring_idx]);
    return OPRT_OK;
}
