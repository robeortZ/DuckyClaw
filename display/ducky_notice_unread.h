/**
 * @file ducky_notice_unread.h
 * @brief Unread count for cloud notice (DP101) badge on home
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef DUCKY_NOTICE_UNREAD_H
#define DUCKY_NOTICE_UNREAD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Increment unread when a new notice is accepted (same as popup)
 * @return none
 */
void ducky_notice_unread_add_one(void);

/**
 * @brief Clear unread (open notice page from home: tap bell or pull-down)
 * @return none
 */
void ducky_notice_unread_clear(void);

/**
 * @brief Current unread count
 * @return Count (0 = no badge)
 */
int ducky_notice_unread_get(void);

#ifdef __cplusplus
}
#endif

#endif /* DUCKY_NOTICE_UNREAD_H */
