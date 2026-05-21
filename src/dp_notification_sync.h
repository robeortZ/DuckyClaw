/**
 * @file dp_notification_sync.h
 * @brief Sync DP101 notification pipe strings into cron and todo persistence
 * @version 0.1
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __DP_NOTIFICATION_SYNC_H__
#define __DP_NOTIFICATION_SYNC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse pipe-formatted notification lines and update cron/todo stores
 * @param[in] raw Multiline string from DP101 (scheduled|… / todo|… lines)
 */
void ducky_dp_notification_sync(const char *raw);

#ifdef __cplusplus
}
#endif

#endif /* __DP_NOTIFICATION_SYNC_H__ */
