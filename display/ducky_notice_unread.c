/**
 * @file ducky_notice_unread.c
 * @brief Unread counter for notice badge
 * @version 1.0
 * @date 2026-03-30
 * @copyright Copyright (c) Tuya Inc.
 */
#include "ducky_notice_unread.h"

/* ---------------------------------------------------------------------------
 * Macros
 * --------------------------------------------------------------------------- */
#ifndef DUCKY_NOTICE_UNREAD_MAX
#define DUCKY_NOTICE_UNREAD_MAX 999
#endif

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
static int s_unread_count;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */
/**
 * @brief Increment unread when a new notice is accepted
 * @return none
 */
void ducky_notice_unread_add_one(void)
{
    if (s_unread_count < DUCKY_NOTICE_UNREAD_MAX) {
        s_unread_count++;
    }
}

/**
 * @brief Clear unread
 * @return none
 */
void ducky_notice_unread_clear(void)
{
    s_unread_count = 0;
}

/**
 * @brief Current unread count
 * @return Count
 */
int ducky_notice_unread_get(void)
{
    return s_unread_count;
}
